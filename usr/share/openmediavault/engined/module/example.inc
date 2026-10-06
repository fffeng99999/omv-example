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

namespace Engined\Module;

/**
 * Example engine module (template skeleton).
 *
 * Marks the module dirty whenever the settings change so that
 * 'Apply changes' re-runs the Salt state (render the systemd unit /
 * config files or the Compose stack files and manage them), and
 * reports the service status to the global Services monitoring
 * (dashboard).
 */
class Example extends \OMV\Engine\Module\ServiceAbstract
	implements \OMV\Engine\Notify\IListener, \OMV\Engine\Module\IServiceStatus
{
	public function getName()
	{
		return "example";
	}

	public function getStatus()
	{
		$db = \OMV\Config\Database::getInstance();
		$object = $db->get("conf.service.example");
		$cmd = new \OMV\System\Process("/usr/sbin/omv-example-ctl", [
			"status"
		]);
		$cmd->setRedirect2to1();
		$cmd->setQuiet(true);
		$output = [];
		$exitStatus = 0;
		$cmd->execute($output, $exitStatus);
		$running = (trim(implode("\n", $output)) === "running");
		return [
			"name" => $this->getName(),
			"title" => gettext("Example"),
			"enabled" => $object->get("enable"),
			"running" => $running
		];
	}

	public function bindListeners(\OMV\Engine\Notify\Dispatcher $dispatcher)
	{
		$dispatcher->addListener(
			OMV_NOTIFY_MODIFY,
			"org.openmediavault.conf.service.example",
			[$this, "setDirty"]
		);
		// Compose mode renders into a shared folder selected by
		// reference, so a modified shared folder must mark the
		// service dirty when it is the referenced one.
		$dispatcher->addListener(
			OMV_NOTIFY_MODIFY,
			"org.openmediavault.conf.system.sharedfolder",
			[$this, "onSharedFolder"]
		);
	}

	public function onSharedFolder($object)
	{
		$db = \OMV\Config\Database::getInstance();
		if ($db->exists("conf.service.example", [
			"operator" => "stringEquals",
			"arg0" => "composeDirRef",
			"arg1" => $object["uuid"]
		])) {
			$this->setDirty();
		}
	}
}
