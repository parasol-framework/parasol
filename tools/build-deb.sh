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
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  -h, --help              Show this help message"
    echo "  -c, --clean             Clean build directory before building"
    echo "  -d, --dev               Build development package (includes headers)"
    echo "  -s, --sign              Sign the package (requires GPG setup)"
    echo "  -b, --build-dir DIR     Use custom build directory (default: build-deb)"
    echo ""
    echo "Examples:"
    echo "  $0                      Build runtime package only"
    echo "  $0 --dev               Build both runtime and development packages"
    echo "  $0 --clean --dev       Clean build and create both packages"
}

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

if ! command -v cmake &> /dev/null; then
    echo "Error: cmake is required but not installed."
    exit 1
fi

# Get version from CMakeLists.txt
VERSION=$(grep "project (Parasol VERSION" CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/')
if [ -z "$VERSION" ]; then
    echo "Error: Could not extract version from CMakeLists.txt"
    exit 1
fi

# Detect architecture
ARCH=$(dpkg --print-architecture)

echo "Building Parasol Debian packages..."
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

# Configure CMake with Debian package generation
echo "Configuring CMake..."
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DGENERATE_DEBIAN_PKG=ON \
    -DPARASOL_STATIC=ON \
    -DRUN_ANYWHERE=OFF \
    -DBUILD_TESTS=OFF \
    -DBUILD_DEFS=OFF \
    -DINSTALL_EXAMPLES=OFF \
    -DINSTALL_INCLUDES="$INSTALL_INCLUDES" \
    -DINSTALL_TESTS=OFF

# Build the project
echo "Building project..."
cmake --build "$BUILD_DIR" --config Release -j $(nproc)

# Create staging directories for packages
RUNTIME_STAGING="$BUILD_DIR/debian/parasol"
DEV_STAGING="$BUILD_DIR/debian/parasol-dev"

echo "Creating package staging directories..."
rm -rf "$RUNTIME_STAGING" "$DEV_STAGING"
mkdir -p "$RUNTIME_STAGING/DEBIAN" "$RUNTIME_STAGING/usr"

if [ "$INSTALL_INCLUDES" = "ON" ]; then
    mkdir -p "$DEV_STAGING/DEBIAN" "$DEV_STAGING/usr"
fi

# Install to staging directory
echo "Installing to staging directory..."
DESTDIR="$RUNTIME_STAGING" cmake --install "$BUILD_DIR"

# Copy debian control files for runtime package
cp "$BUILD_DIR/debian/control" "$RUNTIME_STAGING/DEBIAN/"
if [ ! -d "$RUNTIME_STAGING/usr/share/doc/parasol/" ]; then
    mkdir -p "$RUNTIME_STAGING/usr/share/doc/parasol/" || { echo "Failed to create directory $RUNTIME_STAGING/usr/share/doc/parasol/"; exit 1; }
fi
cp "$BUILD_DIR/debian/copyright" "$RUNTIME_STAGING/usr/share/doc/parasol/" || { echo "Failed to copy copyright file"; exit 1; }
cp "$BUILD_DIR/debian/changelog" "$RUNTIME_STAGING/usr/share/doc/parasol/"

# Create runtime package control file
cat > "$RUNTIME_STAGING/DEBIAN/control" << EOF
Package: parasol
Version: $VERSION-1
Section: graphics
Priority: optional
Architecture: $ARCH
Depends: libfreetype6, zlib1g
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
Depends: parasol (= $VERSION-1), libfreetype6-dev, zlib1g-dev
Maintainer: Parasol Framework Team <team@parasol-framework.org>
Description: Development files for Parasol framework
 This package contains the header files and development libraries needed
 to compile applications that use the Parasol framework.
EOF

    # Copy documentation to dev package
    mkdir -p "$DEV_STAGING/usr/share/doc/parasol-dev"
    cp "$BUILD_DIR/debian/copyright" "$DEV_STAGING/usr/share/doc/parasol-dev/"
    cp "$BUILD_DIR/debian/changelog" "$DEV_STAGING/usr/share/doc/parasol-dev/"
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