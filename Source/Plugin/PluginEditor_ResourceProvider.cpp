#include "PluginEditor_ResourceProvider.h"

namespace ABDMS2000 {

static juce::String getMimeTypeForExtension(const juce::String& ext)
{
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js" || ext == ".mjs") return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".ttf") return "font/ttf";
    if (ext == ".woff2") return "font/woff2";
    if (ext == ".wasm") return "application/wasm";
    return "application/octet-stream";
}

WebUIResourceProvider::WebUIResourceProvider()
{
    // Try to find WebUI folder relative to current source directory or executable
    devRootDirectory_ = juce::File("D:/desarrollos/ABDSynths/ABDMS2000/WebUI");
    if (!devRootDirectory_.exists())
    {
        // Fallback relative to executable directory
        auto currentExeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        devRootDirectory_ = currentExeDir.getChildFile("../../../WebUI");
        if (!devRootDirectory_.exists())
        {
            devRootDirectory_ = currentExeDir.getChildFile("WebUI");
        }
    }
}

std::optional<juce::WebBrowserComponent::Resource> WebUIResourceProvider::getResource(const juce::String& url)
{
    juce::String cleanUrl = url;
    if (cleanUrl.startsWithChar('/'))
        cleanUrl = cleanUrl.substring(1);
    if (cleanUrl.isEmpty() || cleanUrl == "/")
        cleanUrl = "index.html";

    // Remove query parameters or hash
    int queryIndex = cleanUrl.indexOfChar('?');
    if (queryIndex != -1) cleanUrl = cleanUrl.substring(0, queryIndex);
    int hashIndex = cleanUrl.indexOfChar('#');
    if (hashIndex != -1) cleanUrl = cleanUrl.substring(0, hashIndex);

    juce::File targetFile = devRootDirectory_.getChildFile(cleanUrl);
    if (targetFile.existsAsFile())
    {
        juce::MemoryBlock block;
        targetFile.loadFileAsData(block);

        std::vector<std::byte> data(block.getSize());
        std::memcpy(data.data(), block.getData(), block.getSize());

        juce::String ext = targetFile.getFileExtension().toLowerCase();
        juce::String mimeType = getMimeTypeForExtension(ext);

        return juce::WebBrowserComponent::Resource{
            std::move(data),
            mimeType.toStdString()
        };
    }

    return std::nullopt;
}

} // namespace ABDMS2000
