# Fluid Language Support for VS Code

Language support for Fluid scripting (Parasol Framework), including syntax highlighting and LSP integration.

## Setup

### 1. Install Dependencies

```bash
cd tools/vscode-fluid
npm install
```

### 2. Start the LSP Server

Before using LSP features, start the Fluid LSP server:

```bash
parasol tools/lsp_server.fluid port=5007
```

### 3. Install the Extension

**Option A: Install via VS Code Command Palette (Recommended)**

1. Package the extension:
   ```bash
   cd tools/vscode-fluid
   npx vsce package
   ```

2. In VS Code, press `Ctrl+Shift+P`
3. Type **"Extensions: Install from VSIX"** and select it
4. Navigate to `tools/vscode-fluid/fluid-language-0.1.0.vsix`
5. Click Install

**Option B: Install From Command Line**

```bash
# Package the extension
npx vsce package

# Install the .vsix file (requires 'code' in PATH)
code --install-extension fluid-language-0.1.0.vsix
```

## Troubleshooting

### "Could not connect to server"

1. Ensure the LSP server is running.
2. Check the port matches your VS Code settings
3. View the Fluid LSP output channel for connection details:
   - View > Output > Select "Fluid LSP" from dropdown

### No Syntax Highlighting

Ensure the opened file has a `.fluid` extension.
In the bottom right corner, ensure that the Language Mode is set to Fluid.
