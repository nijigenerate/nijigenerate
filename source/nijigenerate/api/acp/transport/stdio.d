module nijigenerate.api.acp.transport.stdio;

import mcp.transport.stdio; // Reuse proven stdio transport from mcp package.

public alias Transport = mcp.transport.stdio.Transport;
public alias StdioTransport = mcp.transport.stdio.StdioTransport;

/// Factory helper to create a line-buffered stdio transport.
StdioTransport createStdioTransport() {
    return mcp.transport.stdio.createStdioTransport();
}
