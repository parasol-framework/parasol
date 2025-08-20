#!/bin/bash
# Parasol Debian Package Builder
# This script builds .deb packages for Parasol Framework

set -e

# Default values
BUILD_DIR="build-deb"
INSTALL_INCLUDES="OFF"
CLEAN_BUILD="no"
SIGN_PACKAGE="no"

# Function to show usage
usage() {
    echo "Usage: $0 <tarball> [options]"
    echo "Arguments:"
    echo "  tarball                 Path to .tar.gz file containing compiled Parasol release"
    echo "Options:"
    echo "  -h, --help              Show this help message"
    echo "  -c, --clean             Clean build directory before building"
    echo "  -d, --dev               Build development package (includes headers)"
    echo "  -s, --sign              Sign the package (requires GPG setup)"
    echo "  -b, --build-dir DIR     Use custom build directory (default: build-deb)"
    echo ""
    echo "Examples:"
    echo "  $0 parasol-linux64-20250731.tar.gz          Build runtime package"
    echo "  $0 parasol-linux64-20250731.tar.gz --dev    Build both runtime and dev packages"
    echo "  $0 parasol-linux64-20250731.tar.gz --clean  Clean build and create package"
}

# Check if tarball is provided
if [[ $# -eq 0 ]]; then
    echo "Error: No tarball specified"
    usage
    exit 1
fi

TARBALL="$1"
shift

# Validate tarball exists
if [[ ! -f "$TARBALL" ]]; then
    echo "Error: Tarball '$TARBALL' not found"
    exit 1
fi

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -c|--clean)
            CLEAN_BUILD="yes"
            shift
            ;;
        -d|--dev)
            INSTALL_INCLUDES="ON"
            shift
            ;;
        -s|--sign)
            SIGN_PACKAGE="yes"
            shift
            ;;
        -b|--build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Check for required tools
if ! command -v dpkg-deb &> /dev/null; then
    echo "Error: dpkg-deb is required but not installed."
    echo "Install it with: sudo apt install dpkg-dev"
    exit 1
fi

if ! command -v tar &> /dev/null; then
    echo "Error: tar is required but not installed."
    exit 1
fi

# Extract version from tarball filename
TARBALL_BASENAME=$(basename "$TARBALL" .tar.gz)
VERSION=$(echo "$TARBALL_BASENAME" | sed -n 's/.*-\([0-9]\{8\}\).*/\1/p')
DATE_VERSION=$(echo "$TARBALL_BASENAME" | sed -n 's/.*-\([0-9]\{8\}\).*/\1/p')
VERSION=""
if [ -z "$DATE_VERSION" ]; then
    # Fallback: try to get version from CMakeLists.txt if available
    if [ -f "CMakeLists.txt" ]; then
        VERSION=$(grep "project (Parasol VERSION" CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/')
    fi
    if [ -z "$VERSION" ]; then
        echo "Error: Could not extract version from tarball filename or CMakeLists.txt"
        echo "Expected tarball format: parasol-linux64-YYYYMMDD.tar.gz"
        exit 1
    fi
fi

# Detect architecture
ARCH=$(dpkg --print-architecture)

echo "Building Parasol Debian packages from tarball..."
echo "Tarball: $TARBALL"
echo "Version: $VERSION"
echo "Architecture: $ARCH"
echo "Build directory: $BUILD_DIR"
echo "Include headers: $INSTALL_INCLUDES"

# Clean build directory if requested
if [ "$CLEAN_BUILD" = "yes" ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"

# Extract tarball to build directory
echo "Extracting tarball..."
mkdir -p "$BUILD_DIR/extracted"
tar -xzf "$TARBALL" -C "$BUILD_DIR/extracted" --strip-components=1

echo "Tarball extracted to $BUILD_DIR/extracted"

# Create staging directories for packages
RUNTIME_STAGING="$BUILD_DIR/debian/parasol"
DEV_STAGING="$BUILD_DIR/debian/parasol-dev"

echo "Creating package staging directories..."
rm -rf "$RUNTIME_STAGING" "$DEV_STAGING"
mkdir -p "$RUNTIME_STAGING/DEBIAN" "$RUNTIME_STAGING/usr"

if [ "$INSTALL_INCLUDES" = "ON" ]; then
    mkdir -p "$DEV_STAGING/DEBIAN" "$DEV_STAGING/usr"
fi

# Copy files from extracted tarball to staging directory
echo "Installing to staging directory..."

# Create necessary directories
mkdir -p "$RUNTIME_STAGING/usr/bin"
mkdir -p "$RUNTIME_STAGING/usr/share/parasol"
mkdir -p "$RUNTIME_STAGING/usr/share/doc/parasol"

# Copy the main parasol binary
cp "$BUILD_DIR/extracted/parasol" "$RUNTIME_STAGING/usr/bin/"
chmod 755 "$RUNTIME_STAGING/usr/bin/parasol"

# Copy configuration and scripts
cp -r "$BUILD_DIR/extracted/config" "$RUNTIME_STAGING/usr/share/parasol/"
cp -r "$BUILD_DIR/extracted/scripts" "$RUNTIME_STAGING/usr/share/parasol/"

# Copy examples if they exist
if [ -d "$BUILD_DIR/extracted/examples" ]; then
    cp -r "$BUILD_DIR/extracted/examples" "$RUNTIME_STAGING/usr/share/parasol/"
fi

# Create copyright and changelog files
cat > "$RUNTIME_STAGING/usr/share/doc/parasol/copyright" << 'EOF'
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: Parasol Framework
Upstream-Contact: Parasol Framework Team <team@parasol-framework.org>
Source: https://github.com/parasol-framework/parasol

Files: *
Copyright: Parasol Framework Team
License: Custom
 This package is distributed under the terms described in the LICENSE.TXT file
 that is distributed with this package. Please refer to it for further
 information on licensing.
EOF

cat > "$RUNTIME_STAGING/usr/share/doc/parasol/changelog" << EOF
parasol ($VERSION-1) stable; urgency=medium

  * Packaged from compiled release tarball

 -- Parasol Framework Team <team@parasol-framework.org>  $(date -R)
EOF

# Create runtime package control file
cat > "$RUNTIME_STAGING/DEBIAN/control" << EOF
Package: parasol
Version: $VERSION-1
Section: graphics
Priority: optional
Architecture: $ARCH
Depends: libasound2, libx11-6, libxext6, libxrandr2, libstdc++6
Maintainer: Parasol Framework Team <team@parasol-framework.org>
Description: Vector graphics engine and application framework
 Parasol is a vector graphics engine and application framework designed for
 creating scalable user interfaces. The framework automatically handles display
 resolution and scaling concerns, allowing developers to focus on application
 logic rather than display technicalities.
EOF

# Handle development package if requested
if [ "$INSTALL_INCLUDES" = "ON" ]; then
    echo "Setting up development package..."
    
    # Move headers and development files to dev package
    if [ -d "$RUNTIME_STAGING/usr/include" ]; then
        mkdir -p "$DEV_STAGING/usr"
        mv "$RUNTIME_STAGING/usr/include" "$DEV_STAGING/usr/"
    fi
    
    # Create dev package control file
    cat > "$DEV_STAGING/DEBIAN/control" << EOF
Package: parasol-dev
Version: $VERSION-1
Section: libdevel
Priority: optional
Architecture: $ARCH
Depends: parasol (= $VERSION-1), libasound2-dev, libx11-dev, libxext-dev, libxrandr-dev
Maintainer: Parasol Framework Team <team@parasol-framework.org>
Description: Development files for Parasol framework
 This package contains the header files and development libraries needed
 to compile applications that use the Parasol framework.
EOF

    # Copy documentation to dev package
    mkdir -p "$DEV_STAGING/usr/share/doc/parasol-dev"
    
    # Create copyright file for dev package
    cat > "$DEV_STAGING/usr/share/doc/parasol-dev/copyright" << 'EOF'
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: Parasol Framework
Upstream-Contact: Parasol Framework Team <team@parasol-framework.org>
Source: https://github.com/parasol-framework/parasol

Files: *
Copyright: Parasol Framework Team
License: Custom
 This package is distributed under the terms described in the LICENSE.TXT file
 that is distributed with this package. Please refer to it for further
 information on licensing.
EOF

    # Create changelog file for dev package
    cat > "$DEV_STAGING/usr/share/doc/parasol-dev/changelog" << EOF
parasol-dev ($VERSION-1) stable; urgency=medium

  * Development package for compiled release tarball

 -- Parasol Framework Team <team@parasol-framework.org>  $(date -R)
EOF
fi

# Build the packages
echo "Building .deb packages..."

# Build runtime package
RUNTIME_PACKAGE="parasol_${VERSION}-1_${ARCH}.deb"
dpkg-deb --build "$RUNTIME_STAGING" "$RUNTIME_PACKAGE"
echo "Created: $RUNTIME_PACKAGE"

# Build development package if requested
if [ "$INSTALL_INCLUDES" = "ON" ]; then
    DEV_PACKAGE="parasol-dev_${VERSION}-1_${ARCH}.deb"
    dpkg-deb --build "$DEV_STAGING" "$DEV_PACKAGE"
    echo "Created: $DEV_PACKAGE"
fi

# Sign packages if requested
if [ "$SIGN_PACKAGE" = "yes" ]; then
    if command -v dpkg-sig &> /dev/null; then
        echo "Signing packages..."
        dpkg-sig --sign builder "$RUNTIME_PACKAGE"
        if [ "$INSTALL_INCLUDES" = "ON" ]; then
            dpkg-sig --sign builder "$DEV_PACKAGE"
        fi
        echo "Packages signed successfully"
    else
        echo "Warning: dpkg-sig not found, packages not signed"
        echo "Install with: sudo apt install dpkg-sig"
    fi
fi

# Verify packages
echo "Verifying packages..."
dpkg-deb -I "$RUNTIME_PACKAGE"
if [ "$INSTALL_INCLUDES" = "ON" ]; then
    dpkg-deb -I "$DEV_PACKAGE"
fi

echo ""
echo "Debian packages built successfully!"
echo "Runtime package: $RUNTIME_PACKAGE"
if [ "$INSTALL_INCLUDES" = "ON" ]; then
    echo "Development package: $DEV_PACKAGE"
fi
echo ""
echo "To install:"
echo "  sudo dpkg -i $RUNTIME_PACKAGE"
if [ "$INSTALL_INCLUDES" = "ON" ]; then
    echo "  sudo dpkg -i $DEV_PACKAGE"
fi
echo ""
echo "To install dependencies if needed:"
echo "  sudo apt-get install -f"