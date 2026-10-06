# This file is part of OpenMediaVault.
#
# @license   https://www.gnu.org/licenses/gpl.html GPL Version 3
# @author    ${GITHUB_USER}
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
#   - compose: renders .env and docker-compose.yml into the compose
#     directory and manages the stack via 'docker compose'
#     (immich style container application).
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
# The compose directory is a shared folder reference (UUID). Resolve
# it with the omv_conf module; an empty reference (nothing selected)
# leaves 'dir' empty and the compose branch renders nothing.
{% set dirref = config.composeDirRef | default('', true) %}
{% if dirref %}
{% set dir = salt['omv_conf.get_sharedfolder_path'](dirref) %}
{% else %}
{% set dir = '' %}
{% endif %}

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

{% if dir %}

render_example_env:
  file.managed:
    - name: '{{ dir }}/.env'
    - source:
      - salt://{{ tpldir }}/files/example.env.j2
    - template: jinja
    - context:
        config: {{ config | json }}
    - user: root
    - group: root
    - mode: '0600'
    - makedirs: True

render_example_compose:
  file.managed:
    - name: '{{ dir }}/docker-compose.yml'
    - source:
      - salt://{{ tpldir }}/files/example.compose.yml.j2
    - template: jinja
    - context:
        config: {{ config | json }}
    - user: root
    - group: root
    - mode: '0644'
    - makedirs: True

{% if config.enable | to_bool %}

# Recreate the stack when the rendered files changed (settings change).
# Explicit deployments are done with the plugin buttons so the image
# pull output is visible to the user.
example_compose_up:
  cmd.run:
    - name: docker compose up -d --remove-orphans
    - cwd: '{{ dir }}'
    - onchanges:
      - file: render_example_env
      - file: render_example_compose

{% else %}

# Disabled: remove the containers but never touch the data. Skipped
# when the stack was never rendered.
example_compose_down:
  cmd.run:
    - name: docker compose down --remove-orphans
    - cwd: '{{ dir }}'
    - onlyif: test -f '{{ dir }}/docker-compose.yml'

{% endif %}

{% else %}

# Compose mode without a shared folder selected: nothing to render.
example_compose_dir_missing:
  test.configurable_test_state:
    - name: example_compose_dir_missing
    - changes: False
    - result: True
    - comment: "Select a shared folder for the stack files on the Example settings page first."

{% endif %}

{% else %}

########################################################################
# systemd mode
########################################################################

# Make sure no compose stack is left over from a mode switch.
example_compose_down_on_switch:
  cmd.run:
    - name: docker compose down --remove-orphans
    - cwd: '{{ dir }}'
    - onlyif: test -f '{{ dir }}/docker-compose.yml'

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
