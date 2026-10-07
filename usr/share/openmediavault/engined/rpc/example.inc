<?php

/**
 * This file is part of OpenMediaVault.
 *
 * @license   https://www.gnu.org/licenses/gpl.html GPL Version 3
 * @author    ${GITHUB_USER} <${GITHUB_USER}@users.noreply.github.com>
 *
 * OpenMediaVault is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenMediaVault is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

namespace Engined\Rpc;

/**
 * Example service RPC (template skeleton).
 *
 * Settings (conf.service.example) live in the OMV configuration
 * database and are read/written through the Config RPC. Lifecycle
 * operations (start/stop/restart) and the log viewer run
 * /usr/sbin/omv-example-ctl as a background task with live output;
 * the ctl script branches on the configured integration mode
 * (systemd service vs. Docker Compose stack).
 *
 * Compose mode: the stack is owned by the openmediavault-compose
 * plugin. On save this RPC publishes (or updates) the stack through
 * Compose.setFile, so it follows the Compose plugin conventions
 * (<compose shared folder>/example/example.yml + example.env) and
 * shows up in the Compose web UI. Switching back to systemd mode
 * removes the registration again (Compose.deleteFile). The plugin
 * never renders the stack files itself, so the Compose plugin stays
 * the single writer. See omv-immich 8.0.7 for a full-blown version of
 * this pattern.
 */
class OMVRpcServiceExample extends \OMV\Rpc\ServiceAbstract
{
    const CTL = "/usr/sbin/omv-example-ctl";

    /**
     * Name of the stack in the openmediavault-compose plugin (Compose
     * mode). It is also the stack directory below the compose shared
     * folder and the base name of the generated stack files.
     */
    const STACK_NAME = "example";

    /**
     * Settings fields allowed to be persisted by set(). Everything else
     * sent by the form (read-only status fields) is stripped.
     */
    private $settingsFields = [
        "enable", "mode", "port", "extraOptions", "image"
    ];

    public function getName()
    {
        return "Example";
    }

    public function initialize()
    {
        $this->registerMethod("get");
        $this->registerMethod("set");
        $this->registerMethod("start");
        $this->registerMethod("stop");
        $this->registerMethod("restart");
        $this->registerMethod("getLog");
    }

    /**
     * Run omv-example-ctl with the given arguments and return the trimmed
     * stdout. Throws on failure when $throwOnError is set.
     */
    private function runCtl(array $args, bool $throwOnError = true): array
    {
        $cmd = new \OMV\System\Process(self::CTL, $args);
        $cmd->setRedirect2to1();
        $cmd->setQuiet(true);
        $output = [];
        $exitStatus = 0;
        $cmd->execute($output, $exitStatus);
        if (0 !== $exitStatus && $throwOnError) {
            throw new \OMV\ExecException($cmd, $output, $exitStatus);
        }
        return [$exitStatus, $output];
    }

    private function getSettingsObject($context): array
    {
        return \OMV\Rpc\Rpc::call("Config", "get", [
            "id" => "conf.service.example"
        ], $context);
    }

    /**
     * Settings object enriched with read-only status information.
     */
    public function get($params, $context)
    {
        $this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);

        $object = $this->getSettingsObject($context);

        // Service state. The status text is returned as the English
        // msgid; the Workbench form page runs it through the
        // 'translate' filter (request.get.transform merges the
        // transformed keys into the response, so all other fields are
        // preserved).
        list(, $statusOut) = $this->runCtl(["status"], false);
        $state = trim(implode("\n", $statusOut));
        if (!in_array($state, ["running", "stopped", "notinstalled"])) {
            $state = "notinstalled";
        }
        $object["installed"] = ("notinstalled" !== $state);
        $object["statusText"] = [
            "running" => "Running",
            "stopped" => "Stopped",
            "notinstalled" => "Not installed"
        ][$state];

        return $object;
    }

    public function set($params, $context)
    {
        $this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
        $data = array_intersect_key($params, array_flip($this->settingsFields));
        $result = \OMV\Rpc\Rpc::call("Config", "set", [
            "id" => "conf.service.example",
            "data" => $data
        ], $context);

        // In Compose mode publish (create or update) the stack in the
        // openmediavault-compose plugin; in systemd mode make sure a
        // stack left over from a previous mode switch is removed.
        $config = $this->getSettingsObject($context);
        if ("compose" === (string)($config["mode"] ?? "systemd")) {
            $this->syncComposeStack($context, $config);
        } else {
            $this->removeComposeStack($context);
        }

        return $result;
    }

    /**
     * Create or update the stack in the openmediavault-compose plugin.
     */
    private function syncComposeStack($context, array $config): void
    {
        $params = [
            "name" => self::STACK_NAME,
            "description" => "Example service (managed by the ".
                "openmediavault-example plugin)",
            "body" => $this->buildComposeBody($config),
            "showenv" => true,
            "env" => $this->buildComposeEnv($config),
            "showoverride" => false,
            "override" => ""
        ];
        $uuid = $this->findStackUuid($context);
        if ("" !== $uuid) {
            $params["uuid"] = $uuid;
        }
        try {
            \OMV\Rpc\Rpc::call("Compose", "setFile", $params, $context);
        } catch (\Exception $e) {
            throw new \OMV\Exception(sprintf(
                "The stack could not be registered in the Compose plugin: ".
                "%s. Install openmediavault-compose and set the shared ".
                "folder for the compose files (Services -> Compose -> ".
                "Settings) first.",
                $e->getMessage()
            ));
        }
    }

    /**
     * Remove the stack from the openmediavault-compose plugin (used when
     * switching back to systemd mode). Best effort: a missing stack or
     * a plugin that is not installed is not an error.
     */
    private function removeComposeStack($context): void
    {
        $uuid = $this->findStackUuid($context);
        if ("" === $uuid) {
            return;
        }
        try {
            \OMV\Rpc\Rpc::call("Compose", "deleteFile", [
                "uuid" => $uuid
            ], $context);
        } catch (\Exception $e) {
            // Ignore - the user can delete the stack on the Compose page.
        }
    }

    /**
     * UUID of the stack in the Compose plugin (empty when it does not
     * exist yet).
     */
    private function findStackUuid($context): string
    {
        $files = \OMV\Rpc\Rpc::call("Config", "get", [
            "id" => "conf.service.compose.file"
        ], $context);
        if (!is_array($files)) {
            return "";
        }
        foreach ($files as $file) {
            if (($file["name"] ?? "") === self::STACK_NAME) {
                return (string)($file["uuid"] ?? "");
            }
        }
        return "";
    }

    /**
     * Docker Compose file body (no auto-generated header; the Compose
     * plugin prepends its own). TODO: replace the 'example' service
     * with the real services of the upstream application.
     */
    private function buildComposeBody(array $config): string
    {
        $body = <<<'YAML'
name: example

services:
  example:
    container_name: example
    image: %%IMAGE%%
    ports:
      - '%%PORT%%:80'
    env_file:
      - example.env
    restart: unless-stopped
YAML;
        $image = trim((string)($config["image"] ?? ""));
        if ("" === $image) {
            $image = "nginx:1.27-alpine";
        }
        $body = strtr($body, [
            "%%IMAGE%%" => $image,
            "%%PORT%%" => (string)($config["port"] ?? "8080")
        ]);
        return $body."\n";
    }

    /**
     * Environment file content for the stack. TODO: replace the keys
     * with the real environment variables of the upstream container.
     */
    private function buildComposeEnv(array $config): string
    {
        $lines = [
            'EXAMPLE_PORT='.(string)($config["port"] ?? "8080"),
            'EXAMPLE_OPTIONS='.(string)($config["extraOptions"] ?? "")
        ];
        return implode("\n", $lines)."\n";
    }

    /**
     * Run an omv-example-ctl operation as a background task (the WebUI
     * shows a task dialog with the live output). This keeps the HTTP
     * request short even for long-running operations.
     */
    private function execCtlTask($context, array $args)
    {
        return $this->execBgProc(
            function ($bgStatusFilename, $bgOutputFilename) use ($args) {
                $cmd = new \OMV\System\Process(self::CTL, $args);
                $cmd->setRedirect2to1();
                if (0 !== ($exitStatus = $this->exec(
                    $cmd, $output, $bgOutputFilename
                ))) {
                    throw new \OMV\ExecException($cmd, $output, $exitStatus);
                }
                return $output;
            }
        );
    }

    public function start($params, $context)
    {
        $this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
        return $this->execCtlTask($context, ["start"]);
    }

    public function stop($params, $context)
    {
        $this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
        return $this->execCtlTask($context, ["stop"]);
    }

    public function restart($params, $context)
    {
        $this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
        return $this->execCtlTask($context, ["restart"]);
    }

    public function getLog($params, $context)
    {
        $this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
        return $this->execCtlTask($context, ["logs"]);
    }
}
