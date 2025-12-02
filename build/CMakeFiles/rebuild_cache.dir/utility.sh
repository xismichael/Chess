set -e

cd /Users/michaelxi/Projects/CMPM123/Chess/build
/opt/homebrew/bin/cmake --regenerate-during-build -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR)
