default:
    @just --list

slides:
    #!/usr/bin/env bash
    set -euo pipefail
    cd _slides
    just clean && just compile
