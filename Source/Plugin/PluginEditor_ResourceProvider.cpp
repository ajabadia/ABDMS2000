#include "PluginEditor_ResourceProvider.h"
#include <juce_core/juce_core.h>
#include "WebUIAssets.h"

namespace ABDMS2000 {

static juce::String getMimeTypeForFilename(const juce::String& filename)
{
    if (filename.endsWithIgnoreCase(".html")) return "text/html";
    if (filename.endsWithIgnoreCase(".css"))  return "text/css";
    if (filename.endsWithIgnoreCase(".js") || filename.endsWithIgnoreCase(".mjs")) return "application/javascript";
    if (filename.endsWithIgnoreCase(".png"))  return "image/png";
    if (filename.endsWithIgnoreCase(".jpg") || filename.endsWithIgnoreCase(".jpeg")) return "image/jpeg";
    if (filename.endsWithIgnoreCase(".svg"))  return "image/svg+xml";
    if (filename.endsWithIgnoreCase(".ttf"))  return "font/ttf";
    if (filename.endsWithIgnoreCase(".woff")) return "font/woff";
    if (filename.endsWithIgnoreCase(".woff2")) return "font/woff2";
    if (filename.endsWithIgnoreCase(".json")) return "application/json";
    if (filename.endsWithIgnoreCase(".webmanifest")) return "application/manifest+json";
    return "application/octet-stream";
}

std::optional<juce::WebBrowserComponent::Resource> pluginResourceProvider(const juce::String& url)
{
    // Development mode: derive paths from source tree
    juce::File thisFile(__FILE__);
    juce::File projectDir = thisFile.getParentDirectory() // Source/Plugin/
                                    .getParentDirectory() // Source/
                                    .getParentDirectory(); // ABDMS2000/
    juce::File webUiDir = projectDir.getChildFile("WebUI");

    juce::String path = url;

    // Strip scheme and host if present (e.g. juce://backend/path -> /path)
    if (path.startsWith("juce://"))
    {
        int hostEndIndex = path.indexOf(7, "/");
        if (hostEndIndex != -1)
            path = path.substring(hostEndIndex);
        else
            path = "/"; // juce://backend -> /
    }
    else if (path.startsWith("https://juce.backend")) path = path.substring(20);
    else if (path.startsWith("http://localhost"))     path = path.substring(16);
    else if (path.startsWith("https://localhost"))    path = path.substring(17);

    if (path == "/" || path.isEmpty()) path = "/index.html";
    if (path.startsWith("/")) path = path.substring(1);

    // URL-decode the path (handles spaces encoded as %20, etc.)
    juce::String decodedPath = juce::URL::removeEscapeChars(path);

    if (decodedPath == "juce.js" || decodedPath.endsWith("/juce.js"))
    {
        return std::nullopt; // Let JUCE WebBrowserComponent serve its built-in frontend script
    }

    // 1. Try loading from disk (Development Mode / Local Source)
    juce::File file = webUiDir.getChildFile(decodedPath.replace("/", "\\"));
    bool fileExists = file.existsAsFile();

    if (!fileExists)
    {
        juce::File fallbackFile = projectDir.getChildFile(decodedPath.replace("/", "\\"));
        if (fallbackFile.existsAsFile())
        {
            file = fallbackFile;
            fileExists = true;
        }
    }

    if (fileExists)
    {
        juce::MemoryBlock mb;
        file.loadFileAsData(mb);
        std::vector<std::byte> data(mb.getSize());
        std::memcpy(data.data(), mb.getData(), mb.getSize());
        return juce::WebBrowserComponent::Resource { std::move(data), getMimeTypeForFilename(file.getFileName()).toStdString() };
    }

    // 2. Fallback to BinaryData (Release / Distribution Mode)
    juce::String resourceName = decodedPath.replace("/", "_")
                                           .replace(".", "_")
                                           .replace("-", "_")
                                           .replace(" ", "_");

    int binSize = 0;
    const char* binData = WebUIAssets::getNamedResource(resourceName.toRawUTF8(), binSize);

    // Numeric name mangling fallback
    if (binData == nullptr && decodedPath.length() > 0)
    {
        juce::String filename = decodedPath.fromLastOccurrenceOf("/", false, false);
        if (filename.isEmpty()) filename = decodedPath;
        juce::String flattenedName = filename.replace(".", "_").replace("-", "_").replace(" ", "_");
        if (juce::CharacterFunctions::isDigit(flattenedName[0]))
            flattenedName = "_" + flattenedName;
        binData = WebUIAssets::getNamedResource(flattenedName.toRawUTF8(), binSize);
    }

    if (binData != nullptr)
    {
        std::vector<std::byte> bytes(binSize);
        std::memcpy(bytes.data(), binData, (size_t)binSize);
        return juce::WebBrowserComponent::Resource { std::move(bytes), getMimeTypeForFilename(decodedPath).toStdString() };
    }

    // Log missing file for debug
    juce::Logger::writeToLog("[ResourceProvider] ERROR: File not found on disk or BinaryData for path: " + decodedPath);

    return std::nullopt;
}

} // namespace ABDMS2000
