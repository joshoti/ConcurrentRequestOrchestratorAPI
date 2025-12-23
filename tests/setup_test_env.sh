#!/bin/bash
# Setup script for Python integration test environment

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$SCRIPT_DIR/venv"

echo "🐍 Setting up Python test environment..."

# Check if Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo "❌ Python 3 is not installed. Please install Python 3 first."
    exit 1
fi

# Create virtual environment if it doesn't exist
if [ ! -d "$VENV_DIR" ]; then
    echo "📦 Creating virtual environment..."
    python3 -m venv "$VENV_DIR"
    echo "✅ Virtual environment created at $VENV_DIR"
else
    echo "✅ Virtual environment already exists"
fi

# Activate virtual environment
echo "🔌 Activating virtual environment..."
source "$VENV_DIR/bin/activate"

# Upgrade pip
echo "⬆️  Upgrading pip..."
pip install --upgrade pip > /dev/null 2>&1

# Install dependencies
echo "📥 Installing dependencies..."
pip install -r "$SCRIPT_DIR/requirements.txt" > /dev/null 2>&1

echo ""
echo "✅ Setup complete!"
echo ""
echo "To run tests automatically:"
echo "   bash tests/run_integration_tests.sh"
echo ""
echo "To activate the virtual environment manually, run:"
echo "   source tests/venv/bin/activate"
echo ""
echo "To deactivate manually:"
echo "   deactivate"
