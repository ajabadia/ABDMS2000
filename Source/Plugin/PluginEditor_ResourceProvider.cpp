#include "PluginEditor_ResourceProvider.h"
#include <juce_core/juce_core.h>
#include "WebUIAssets.h"
#include "../Core/AppLogger.h"

namespace ABDMS2000
{

static juce::String getMimeTypeForFilename(const juce::String& filename)
{
    if (filename.endsWithIgnoreCase(".html")) return "text/html";
    if (filename.endsWithIgnoreCase(".css"))  return "text/css";
    if (filename.endsWithIgnoreCase(".js") || filename.endsWithIgnoreCase(".mjs")) return "application/javascript";
    if (filename.endsWithIgnoreCase(".png"))  return "image/png";
    if (filename.endsWithIgnoreCase(".jpg") || filename.endsWithIgnoreCase(".jpeg")) return "image/jpeg";
    if (filename.endsWithIgnoreCase(".webp")) return "image/webp";
    if (filename.endsWithIgnoreCase(".svg"))  return "image/svg+xml";
    if (filename.endsWithIgnoreCase(".ttf"))  return "font/ttf";
    if (filename.endsWithIgnoreCase(".woff")) return "font/woff";
    if (filename.endsWithIgnoreCase(".woff2")) return "font/woff2";
    if (filename.endsWithIgnoreCase(".json")) return "application/json";
    if (filename.endsWithIgnoreCase(".webmanifest")) return "application/manifest+json";
    return "application/octet-stream";
}

static juce::File findABDSharedAssetsRoot()
{
    juce::File exeFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    juce::File dir = exeFile.getParentDirectory();

    for (int i = 0; i < 8 && dir.exists(); ++i)
    {
        juce::File candidate = dir.getChildFile("ABDSharedAssets");
        if (candidate.isDirectory())
            return candidate;
        dir = dir.getParentDirectory();
    }

    juce::File cwd = juce::File::getCurrentWorkingDirectory();
    for (int i = 0; i < 6 && cwd.exists(); ++i)
    {
        juce::File candidate = cwd.getChildFile("ABDSharedAssets");
        if (candidate.isDirectory())
            return candidate;
        cwd = cwd.getParentDirectory();
    }

    juce::File projectRoot = exeFile.getParentDirectory().getParentDirectory().getParentDirectory().getParentDirectory();
    juce::File candidate = projectRoot.getChildFile("ABDSharedAssets");
    if (candidate.isDirectory())
        return candidate;

    return {};
}

std::optional<juce::WebBrowserComponent::Resource> pluginResourceProvider(const juce::String& url)
{
    ABD_LOG(juce::String("[RESOURCE] Request for URL: ") + url);

    juce::String path = url;

    // Handle bankwebui:// protocol - serve from ABDSharedAssets/abdbank junction
    if (url.startsWith("bankwebui://"))
    {
        juce::String bankPath = url.substring(12); // remove "bankwebui://"
        if (bankPath.isEmpty() || bankPath == "/")
            bankPath = "index.html";
        if (bankPath.startsWith("/"))
            bankPath = bankPath.substring(1);

        juce::String decodedPath = juce::URL::removeEscapeChars(bankPath);
        // Strip cache-busting query string (?v=...) so the same file is served regardless of version
        if (decodedPath.containsChar('?'))
            decodedPath = decodedPath.upToFirstOccurrenceOf("?", false, false);

        // Serve from ABDSharedAssets/abdbank junction (which points to ABDBankManager/WebUI)
        juce::File bankRoot;
        if (auto assetsRoot = findABDSharedAssetsRoot(); assetsRoot.isDirectory())
            bankRoot = assetsRoot.getChildFile("abdbank");

        if (!bankRoot.isDirectory())
        {
            juce::File sourceFile(__FILE__);
            auto projectRoot = sourceFile.getParentDirectory().getParentDirectory().getParentDirectory();
            bankRoot = projectRoot.getChildFile("WebUI").getChildFile("abdbank");
            if (!bankRoot.isDirectory())
                bankRoot = projectRoot.getParentDirectory().getChildFile("ABDBankManager").getChildFile("WebUI");
        }

        juce::File file = bankRoot.getChildFile(decodedPath);
        if (file.existsAsFile())
        {
            ABD_LOG(juce::String("[RESOURCE] Serving BankManager WebUI from disk: ") + file.getFullPathName());
            juce::MemoryBlock mb;
            if (file.loadFileAsData(mb))
            {
                std::vector<std::byte> bytes(static_cast<size_t>(mb.getSize()));
                std::memcpy(bytes.data(), mb.getData(), static_cast<size_t>(mb.getSize()));
                return juce::WebBrowserComponent::Resource { std::move(bytes), getMimeTypeForFilename(file.getFileName()).toStdString() };
            }
        }
        return std::nullopt;
    }

    // Strip scheme and host if present (e.g. juce://backend/path -> /path)
    if (path.startsWith("juce://"))
    {
        int hostEndIndex = path.indexOf(7, "/");
        if (hostEndIndex != -1)
            path = path.substring(hostEndIndex);
        else
            path = "/";
    }
    else if (path.startsWith("https://juce.backend")) path = path.substring(20);
    else if (path.startsWith("http://localhost"))     path = path.substring(16);
    else if (path.startsWith("https://localhost"))    path = path.substring(17);

    if (path == "/" || path.isEmpty()) path = "/index.html";
    if (path.startsWith("/")) path = path.substring(1);

    // URL-decode the path (handles spaces encoded as %20, etc.)
    juce::String decodedPath = juce::URL::removeEscapeChars(path);
    // Strip cache-busting query string (?v=...) so the same file is served regardless of version
    if (decodedPath.containsChar('?'))
        decodedPath = decodedPath.upToFirstOccurrenceOf("?", false, false);

    if (decodedPath == "juce.js" || decodedPath.endsWith("/juce.js"))
    {
        ABD_LOG("[RESOURCE] Serving built-in juce.js (std::nullopt).");
        return std::nullopt; // Let JUCE WebBrowserComponent serve its built-in frontend script
    }

    // 1. PRIMARY: Match against WebUIAssets (Binary Data)
    int binSize = 0;
    const char* binData = nullptr;

    // A) Match by iterating originalFilenames in WebUIAssets (prefer exact relative path under WebUI/)
    juce::String normalizedDecoded = decodedPath.replace("\\", "/");
    juce::String exactWebUiPath = "WebUI/" + normalizedDecoded;

    // Pass 1: Exact match with WebUI/ prefix (prevents collisions like src/styles/main.css vs components/bank/src/styles/main.css)
    for (int i = 0; i < WebUIAssets::namedResourceListSize; ++i)
    {
        juce::String orig = juce::String::fromUTF8(WebUIAssets::originalFilenames[i]).replace("\\", "/");
        if (orig.endsWithIgnoreCase("/" + exactWebUiPath) || orig.equalsIgnoreCase(exactWebUiPath) || orig.endsWithIgnoreCase(exactWebUiPath))
        {
            binData = WebUIAssets::getNamedResource(WebUIAssets::namedResourceList[i], binSize);
            if (binData != nullptr)
            {
                ABD_LOG(juce::String("[RESOURCE] Exact match in WebUIAssets: ") + WebUIAssets::namedResourceList[i] + " (" + juce::String(binSize) + " bytes)");
                break;
            }
        }
    }

    // Pass 2: Suffix fallback if Pass 1 did not find it
    if (binData == nullptr)
    {
        for (int i = 0; i < WebUIAssets::namedResourceListSize; ++i)
        {
            juce::String orig = juce::String::fromUTF8(WebUIAssets::originalFilenames[i]).replace("\\", "/");
            if (orig.endsWithIgnoreCase("/" + normalizedDecoded) || orig.equalsIgnoreCase(normalizedDecoded))
            {
                binData = WebUIAssets::getNamedResource(WebUIAssets::namedResourceList[i], binSize);
                if (binData != nullptr)
                {
                    ABD_LOG(juce::String("[RESOURCE] Suffix match in WebUIAssets: ") + WebUIAssets::namedResourceList[i] + " (" + juce::String(binSize) + " bytes)");
                    break;
                }
            }
        }
    }

    // B) Direct flattened names fallback
    if (binData == nullptr)
    {
        juce::String resourceName = decodedPath.replace("/", "_")
                                               .replace("\\", "_")
                                               .replace(".", "_")
                                               .replace("-", "_")
                                               .replace(" ", "_");
        binData = WebUIAssets::getNamedResource(resourceName.toRawUTF8(), binSize);
    }

    // C) Basename matching for Vite-bundled assets
    // juce_add_binary_data stores only basenames in originalFilenames
    // (e.g. "index.css" not "WebUI/dist/assets/index.css"). For Vite output
    // paths like "assets/index.css" the full-path and flattened-name matches
    // both fail, so we extract the basename and try to match it.
    // IMPORTANT: only apply to assets/ paths to avoid matching abdbank/index.html
    // to the synth root index.html (which would cause recursive loading).
    if (binData == nullptr && decodedPath.startsWith("assets/"))
    {
        juce::String basename = decodedPath.fromLastOccurrenceOf("/", false, false);
        if (basename.isNotEmpty())
        {
            juce::String flattenedBasename = basename.replace(".", "_").replace("-", "_").replace(" ", "_");
            binData = WebUIAssets::getNamedResource(flattenedBasename.toRawUTF8(), binSize);
            if (binData != nullptr)
                ABD_LOG(juce::String("[RESOURCE] Assets basename match: ") + flattenedBasename + " (" + juce::String(binSize) + " bytes)");
        }
    }

    // D) Root-level basename fallback
    // Only use a basename fallback for root-level files. Applying it to a
    // missing nested index.html can accidentally return the synth root page,
    // causing the embedded Bank Manager to recursively load ABDMS2000.
    if (binData == nullptr && decodedPath.length() > 0 && !decodedPath.containsChar('/'))
    {
        juce::String flattenedName = decodedPath.replace(".", "_").replace("-", "_").replace(" ", "_");
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

    // 2. Secondary Disk Fallback (Development Mode)
    juce::File thisFile(__FILE__);
    juce::File projectDir = thisFile.getParentDirectory()
                                    .getParentDirectory()
                                    .getParentDirectory();
    juce::File webUiDir = projectDir.getChildFile("WebUI");

    // Requests may be normalized to /abdbank/... by WebView2. Prefer the
    // synchronized copy in this project, then fall back to the sibling repo.
    juce::String diskRel = decodedPath.replace("/", "\\");
    if (decodedPath.startsWith("abdbank/"))
    {
        const auto bankRel = decodedPath.fromFirstOccurrenceOf("abdbank/", false, false);
        const auto localBankDir = projectDir.getChildFile("WebUI").getChildFile("abdbank");
        const auto siblingBankDir = projectDir.getParentDirectory().getChildFile("ABDBankManager").getChildFile("WebUI");
        webUiDir = localBankDir.isDirectory() ? localBankDir : siblingBankDir;
        diskRel = bankRel.replace("/", "\\");
    }

    juce::File file = webUiDir.getChildFile(diskRel);
    if (file.existsAsFile())
    {
        ABD_LOG(juce::String("[RESOURCE] Disk fallback: ") + file.getFullPathName() + " (" + juce::String(file.getSize()) + " bytes)");
        juce::MemoryBlock mb;
        file.loadFileAsData(mb);
        std::vector<std::byte> data(mb.getSize());
        std::memcpy(data.data(), mb.getData(), mb.getSize());
        return juce::WebBrowserComponent::Resource { std::move(data), getMimeTypeForFilename(file.getFileName()).toStdString() };
    }

    ABD_LOG("[RESOURCE] ERROR: File not found in WebUIAssets or disk for path: " + decodedPath);
    return std::nullopt;
}

} // namespace ABDMS2000