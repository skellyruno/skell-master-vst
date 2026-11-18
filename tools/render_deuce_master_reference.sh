#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo 'Usage: ./render_deuce_master_reference.sh <Skelly Master.vst3> <input.wav> <output.wav>' >&2
  exit 2
fi

plugin=$1
input=$2
output=$3
runner="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/SkellyVST3HostAudioTestsLinux"

if [[ ! -d "$plugin" ]]; then
  echo "VST3 bundle not found: $plugin" >&2
  exit 2
fi
if [[ ! -f "$input" ]]; then
  echo "Input WAV not found: $input" >&2
  exit 2
fi
if [[ -e "$output" ]]; then
  echo "Refusing to overwrite existing output: $output" >&2
  exit 2
fi
if [[ ! -x "$runner" ]]; then
  echo "VST3 host runner not found beside this script: $runner" >&2
  exit 2
fi

echo 'Rendering local audio through the compiled Skell Master Linux VST3.'
echo 'This is not an FL Studio render; output includes reported plugin latency and tail.'
"$runner" "$plugin" "$input" "$output"
