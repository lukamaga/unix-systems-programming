#!/bin/bash
case "$1" in http://*|https://*) echo valid;; *) echo invalid; exit 1;; esac
