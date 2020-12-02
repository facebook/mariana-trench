#!/bin/sh

D8=/opt/android/sdk_D23134735/build-tools/29.0.2/d8
$D8 --min-api 24 --output classes_for_analysis.zip "$@"
