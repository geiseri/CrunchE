#!/bin/sh
# Build and run the pairwise stack (intermodulation) test — no hardware needed.
# Renders every melodic-instrument pair in demo-like roles and gates on
# sustained excursions past the master knee.
set -e
cd "$(dirname "$0")/../.."
c++ -std=gnu++20 -O2 -I. \
  tests/native/make_stacks.cpp Voice.cpp \
  -o /tmp/crunche_stacks
/tmp/crunche_stacks
