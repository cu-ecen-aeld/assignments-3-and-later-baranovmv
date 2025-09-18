#!/bin/sh

if [ $# -ne 2 ]; then
  echo "Must be two parameters: writefile writestr"
  exit 1
fi

if ! mkdir -p "$(dirname "$1")"; then
  echo "Failed to create directory" >&2
fi

if ! touch $1; then
  echo "Failed to create file" >&2
fi

echo "$2" >$1
exit 0
