# This file is part of OpenMediaVault.
#
# @license   https://www.gnu.org/licenses/gpl.html GPL Version 3
# @author    ${GITHUB_USER} <${GITHUB_USER}@users.noreply.github.com>
#
# OpenMediaVault is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.

# Template Salt state with two integration modes, selected by the
# 'mode' setting (Services -> Example -> Settings):
#
#   - systemd: renders /etc/example/example.conf and the
#     /etc/systemd/system/example.service unit and manages the unit
#     (frpc/clouddrive2 style host service).
#   - compose: manages the lifecycle of the Docker Compose stack that
#     the RPC registers in the openmediavault-compose plugin. The stack
#     files are rendered BY the Compose plugin (single writer); this
#     state only runs 'omv-compose-run example up/down'.
#
# When deriving a real plugin from this template, KEEP the branch that
# matches your target service and DELETE the other one (together with
# its j2 templates, the datamodel/form fields and the ctl branch).
#
# Rules baked in (do not remove):
#   - every file.managed MUST set '- template: jinja' explicitly;
#   - state IDs must be unique in the RENDERED text, so the mutually
#     exclusive if/else branches use distinct IDs;
#   - conditional execution uses onlyif/unless so missing artifacts
#     never fail the Apply.

{% set config = salt['omv_conf.get']('conf.service.example') %}
{% set mode = config.mode | default('systemd', true) %}

{% if mode == 'compose' %}

########################################################################
# Docker Compose mode
########################################################################

# Make sure no native unit is left over from a mode switch to avoid
# two instances of the service running at the same time.
remove_example_systemd_unit_file:
  file.absent:
    - name: /etc/systemd/system/example.service

example_systemctl_daemon_reload:
  module.run:
    - service.systemctl_reload:
    - onchanges:
      - file: remove_example_systemd_unit_file

# The stack is registered in the openmediavault-compose plugin by the
# RPC on save and rendered there into
#   <compose shared folder>/example/example.yml (+ example.env).
# This state only brings the stack up/down through the Compose plugin
# helper, so exactly the same --file/--env-file arguments are used as
# from the Compose web UI.
{% set compose = salt['omv_conf.get']('conf.service.compose') %}
{% if compose.sharedfolderref | length > 0 %}

{% set sfpath = salt['omv_conf.get_sharedfolder_path'](compose.sharedfolderref).rstrip('/') %}
{% set stackfile = sfpath ~ '/example/example.yml' %}

{% if config.enable | to_bool %}

example_stack_up:
  cmd.run:
    - name: /usr/sbin/omv-compose-run example up -d
    - onlyif: test -f '{{ stackfile }}'

{% else %}

# Disabled: remove the containers but never touch the data. Skipped
# when the stack was never registered.
example_stack_down:
  cmd.run:
    - name: /usr/sbin/omv-compose-run example down --remove-orphans
    - onlyif: test -f '{{ stackfile }}'

{% endif %}

{% else %}

# Compose mode while the Compose plugin shared folder is not configured
# yet: nothing to do (the RPC reports a clear error on save).
example_compose_not_configured:
  test.configurable_test_state:
    - name: example_compose_not_configured
    - changes: False
    - result: True
    - comment: "Configure the Compose plugin (Services -> Compose -> Settings) and select the shared folder for the compose files first."

{% endif %}

{% else %}

########################################################################
# systemd mode
########################################################################

# Note: switching from Compose mode to systemd mode removes the stack
# registration (and stops the containers) in the RPC, not here.

render_example_config:
  file.managed:
    - name: /etc/example/example.conf
    - source:
      - salt://{{ tpldir }}/files/example.conf.j2
    - template: jinja
    - context:
        config: {{ config | json }}
    - user: root
    - group: root
    - mode: '0644'
    - makedirs: True

{% if config.enable | to_bool %}

create_example_systemd_unit_file:
  file.managed:
    - name: /etc/systemd/system/example.service
    - source:
      - salt://{{ tpldir }}/files/example.service.j2
    - template: jinja
    - context:
        config: {{ config | json }}
    - user: root
    - group: root
    - mode: '0644'

example_systemctl_daemon_reload:
  module.run:
    - service.systemctl_reload:
    - onchanges:
      - file: create_example_systemd_unit_file

start_example_service:
  service.running:
    - name: example
    - enable: True
    - watch:
      - file: render_example_config
      - file: create_example_systemd_unit_file

{% else %}

# The native unit only exists while the service is enabled. Salt
# requires every state ID to be unique inside one rendered SLS, so the
# enabled and disabled branches must not share state IDs.
stop_example_service:
  service.dead:
    - name: example
    - enable: False
    - onlyif: test -f /etc/systemd/system/example.service

remove_example_systemd_unit_file:
  file.absent:
    - name: /etc/systemd/system/example.service

example_systemctl_daemon_reload:
  module.run:
    - service.systemctl_reload:
    - onchanges:
      - file: remove_example_systemd_unit_file

{% endif %}

{% endif %}
