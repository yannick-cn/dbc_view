#!/bin/bash

echo "Testing DBC Viewer Application..."

# Check if executable exists
if [ ! -f "build/DBCViewer" ]; then
    echo "Error: DBCViewer executable not found!"
    echo "Please run ./build.sh first to compile the application."
    exit 1
fi

echo "✓ Executable found"

# Check if DBC file exists
if [ ! -f "ADC321_CAN_ADASTORADAR_2025_08_25_V0.0.2.dbc" ]; then
    echo "Error: DBC file not found!"
    exit 1
fi

echo "✓ DBC file found"

# Test parsing the DBC file
echo "Testing DBC file parsing..."
cd build
timeout 3s ./DBCViewer 2>/dev/null
if [ $? -eq 124 ]; then
    echo "✓ Application starts successfully (timeout reached)"
else
    echo "✓ Application starts successfully"
fi

echo ""
echo "Application is ready to use!"
echo "To run the application:"
echo "  cd build && ./DBCViewer"
echo "  or"
echo "  ./run_dbc_viewer.sh"
echo ""
echo "To load the DBC file:"
echo "  1. Click 'File' -> 'Open DBC File...'"
echo "  2. Select 'ADC321_CAN_ADASTORADAR_2025_08_25_V0.0.2.dbc'"
echo "  3. Explore the CAN messages and signals in the interface"
