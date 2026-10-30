#!/bin/sh

mkdir -p data/raw
URL="https://zenodo.org/records/15240501/files"

file_size() {
  if stat -f%z "$1" 2>/dev/null; then
    return
  fi
  if stat -c%s "$1" 2>/dev/null; then
    return
  fi
  echo 0
}

get() {
  filename=$1
  path="data/raw/$filename"
  address="$URL/$filename?download=1"
  total=$(curl -sIL "$address" | grep -i content-length | tail -1 | tr -dc 0-9)
  downloaded=$(file_size "$path")
  while [ "$downloaded" != "$total" ]; do
    if ! curl -sSL -C - -y 30 -Y 10000 -o "$path" "$address"; then
      sleep 5
    fi
    downloaded=$(file_size "$path")
  done
  echo "OK $filename"
}

for f in fb_200M_uint64 wiki_ts_200M_uint64 osm_cellids_800M_uint64 books_800M_uint64 \
         companynet_uint32 exponential_uint32 friendster_50M_uint32 lognormal_uint32 \
         normal_uint32 zipf_uint32 books_50M_uint64; do
  get $f &
done
wait
for f in books_200M_uint32 wiki_ts_200M_uint32 normal_800M_uint32; do
  get $f &
done
wait
