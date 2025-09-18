#!/bin/sh

if [ $# -ne 2 ]; then
  echo "Must be two parameters: filesdir searchstr"
  exit 1
fi

if [ ! -d $1 ]; then
  echo "1st paramter must be a path to a directory"
  exit 1
fi

files=$(find $1 -type f)
file_count=$(echo "$files" | wc -l)
found=$(grep -F $2 $(echo "$files") | wc -l)
echo "The number of files are $file_count and the number of matching lines are $found"
exit 0
