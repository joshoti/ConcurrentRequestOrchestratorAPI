#!/bin/bash
# Run integration tests in virtual environment

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$SCRIPT_DIR/venv"

# Check if virtual environment exists
if [ ! -d "$VENV_DIR" ]; then
    echo "❌ Virtual environment not found. Run setup_test_env.sh first:"
    echo "   bash tests/setup_test_env.sh"
    exit 1
fi

# Activate virtual environment
source "$VENV_DIR/bin/activate"

# Run tests
python3 "$SCRIPT_DIR/test_server_integration.py"

# Deactivate virtual environment
deactivate
