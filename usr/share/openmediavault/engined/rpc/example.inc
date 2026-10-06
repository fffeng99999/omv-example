<?php

/**
 * This file is part of OpenMediaVault.
 *
 * @license   https://www.gnu.org/licenses/gpl.html GPL Version 3
 * @author    ${GITHUB_USER}
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
 */
class OMVRpcServiceExample extends \OMV\Rpc\ServiceAbstract
{
    const CTL = "/usr/sbin/omv-example-ctl";

    /**
     * Settings fields allowed to be persisted by set(). Everything else
     * sent by the form (read-only status fields) is stripped.
     */
    private $settingsFields = [
        "enable", "mode", "port", "extraOptions", "image", "composeDirRef"
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
        return \OMV\Rpc\Rpc::call("Config", "set", [
            "id" => "conf.service.example",
            "data" => $data
        ], $context);
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
