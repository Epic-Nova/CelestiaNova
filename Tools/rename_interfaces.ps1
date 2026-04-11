$root = 'h:\EpicNova\Applications\CelestiaNova\FutureLooking'
$files = Get-ChildItem -Recurse -Path $root -Include '*.h','*.cpp' |
    Where-Object { $_.FullName -notmatch '\\(Intermediate|Binaries|Build)\\' }

$replacements = @(
    [pscustomobject]@{ From = '#include "Core/IModuleInterface.h"';    To = '#include "Core/IExtensionInterface.h"' },
    [pscustomobject]@{ From = '#include "Core/PluginRegistry.h"';      To = '#include "Core/ExtensionRegistry.h"' },
    [pscustomobject]@{ From = 'Core::PluginRegistry::Instance()';      To = 'Core::ExtensionRegistry::Instance()' },
    [pscustomobject]@{ From = 'PluginRegistry::Instance()';            To = 'ExtensionRegistry::Instance()' },
    [pscustomobject]@{ From = '.ListDescriptors()';                    To = '.ListExtensionDescriptors()' },
    [pscustomobject]@{ From = '.HasPlugin(';                           To = '.HasExtension(' },
    [pscustomobject]@{ From = '.GetDescriptor(';                       To = '.GetExtensionDescriptor(' },
    [pscustomobject]@{ From = '.GetDescriptorPath(';                   To = '.GetExtensionDescriptorPath(' },
    [pscustomobject]@{ From = '.GetLoadedModuleInstance(';             To = '.GetLoadedExtensionInstance(' },
    [pscustomobject]@{ From = '.GetLoadedModuleSymbol(';               To = '.GetLoadedExtensionSymbol(' },
    [pscustomobject]@{ From = '.IsLoaded(';                            To = '.IsExtensionLoaded(' },
    [pscustomobject]@{ From = '.LoadPluginById(';                      To = '.LoadExtensionById(' },
    [pscustomobject]@{ From = '.UnloadPluginById(';                    To = '.UnloadExtensionById(' },
    [pscustomobject]@{ From = '.UnloadAll()';                          To = '.UnloadAllExtensions()' },
    [pscustomobject]@{ From = 'public IModuleInterface,';              To = 'public IExtensionInterface,' },
    [pscustomobject]@{ From = 'public IModuleInterface {';             To = 'public IExtensionInterface {' },
    [pscustomobject]@{ From = 'public IModuleInterface{';              To = 'public IExtensionInterface{' },
    [pscustomobject]@{ From = 'class IModuleInterface ';               To = 'class IExtensionInterface ' },
    [pscustomobject]@{ From = 'class IModuleInterface{';               To = 'class IExtensionInterface{' },
    [pscustomobject]@{ From = 'IModuleInterface* ';                    To = 'IExtensionInterface* ' },
    [pscustomobject]@{ From = 'IModuleInterface*,';                    To = 'IExtensionInterface*,' },
    [pscustomobject]@{ From = 'IModuleInterface*)';                    To = 'IExtensionInterface*)' },
    [pscustomobject]@{ From = 'IModuleInterface* GetLoadedModuleInstance'; To = 'IExtensionInterface* GetLoadedExtensionInstance' },
    [pscustomobject]@{ From = 'struct PluginDescriptor';               To = 'struct ExtensionDescriptor' },
    [pscustomobject]@{ From = 'PluginDescriptor>';                     To = 'ExtensionDescriptor>' },
    [pscustomobject]@{ From = 'PluginDescriptor&';                     To = 'ExtensionDescriptor&' },
    [pscustomobject]@{ From = 'PluginDescriptor*';                     To = 'ExtensionDescriptor*' },
    [pscustomobject]@{ From = 'PluginDescriptor ';                     To = 'ExtensionDescriptor ' },
    [pscustomobject]@{ From = 'class PluginRegistry';                  To = 'class ExtensionRegistry' },
    [pscustomobject]@{ From = 'PluginRegistry&';                       To = 'ExtensionRegistry&' }
)

$changed = 0
foreach ($file in $files) {
    # Skip the old files being replaced
    if ($file.Name -eq 'PluginRegistry.h') { continue }
    if ($file.Name -eq 'PluginRegistry.cpp') { continue }
    if ($file.Name -eq 'IModuleInterface.h') { continue }
    # Skip newly created files to avoid double-processing
    if ($file.Name -eq 'ExtensionRegistry.h') { continue }
    if ($file.Name -eq 'ExtensionRegistry.cpp') { continue }
    if ($file.Name -eq 'IExtensionInterface.h') { continue }

    $content = [System.IO.File]::ReadAllText($file.FullName, [System.Text.Encoding]::UTF8)
    $original = $content
    foreach ($r in $replacements) {
        $content = $content.Replace($r.From, $r.To)
    }
    if ($content -ne $original) {
        [System.IO.File]::WriteAllText($file.FullName, $content, [System.Text.Encoding]::UTF8)
        Write-Host "Updated: $($file.FullName)"
        $changed++
    }
}
Write-Host "Done. Files changed: $changed"
