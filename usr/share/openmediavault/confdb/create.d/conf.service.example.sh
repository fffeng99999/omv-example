#!/usr/bin/env dash
#
# This file is part of OpenMediaVault.
#
# @license   https://www.gnu.org/licenses/gpl.html GPL Version 3
# @author    ${GITHUB_USER} <${GITHUB_USER}@users.noreply.github.com>
#
# OpenMediaVault is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.

set -e

. /usr/share/openmediavault/scripts/helper-functions

########################################################################
# Update the configuration.
# <config>
#   <services>
#     <example>
#       <enable>0|1</enable>
#       <mode>systemd|compose</mode>
#       <port>8080</port>
#       <extraOptions>...</extraOptions>
#       <image>nginx:1.27-alpine</image>
#     </example>
#   </services>
# </config>
#
# NOTE: in Compose mode the stack is registered in the Compose plugin
# (conf.service.compose.file) by the RPC on save - there is no stack
# directory field in this plugin any more.
#
# NOTE: defaults here MUST match conf.service.example.json. New fields
# added in a later plugin version need a migrations.d script that adds
# the key to existing installations (see omv-plugin-dev skill).
########################################################################
if ! omv_config_exists "/config/services/example"; then
	omv_config_add_node "/config/services" "example"
	omv_config_add_key "/config/services/example" "enable" "0"
	omv_config_add_key "/config/services/example" "mode" "systemd"
	omv_config_add_key "/config/services/example" "port" "8080"
	omv_config_add_key "/config/services/example" "extraOptions" ""
	omv_config_add_key "/config/services/example" "image" "nginx:1.27-alpine"
fi

exit 0
