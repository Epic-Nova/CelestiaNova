#pragma once

#include "NovaMinimal.h"
#include <filesystem>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <regex>
#include <map>
#include <optional>

namespace Core::FileOperations 
{
    /**
     * Structure to hold file information
     */
    struct NOVA_CORE_API FileInfo {
        std::string path;
        std::string name;
        std::string extension;
        std::uintmax_t size;
        std::filesystem::file_time_type lastModified;
        bool isDirectory;
        
        // Format the last modified time as a string in a standard format
        std::string GetFormattedTime() const;
    };
    
    /**
     * Filter options for file operations
     */
    struct FileFilterOptions {
        std::optional<std::string> extension; // Filter by file extension (e.g., ".txt")
        std::optional<std::string> namePattern; // Filter by filename pattern (supports wildcards * and ?)
        std::optional<std::regex> regexPattern; // Filter by regex pattern
        std::optional<std::uintmax_t> minSize; // Filter by minimum file size
        std::optional<std::uintmax_t> maxSize; // Filter by maximum file size
        std::optional<std::filesystem::file_time_type> modifiedAfter; // Filter by modified after date
        std::optional<std::filesystem::file_time_type> modifiedBefore; // Filter by modified before date
        bool includeHidden = false; // Whether to include hidden files
        bool includeDirectories = true; // Whether to include directories
        bool includeFiles = true; // Whether to include files
    };
    
    /**
     * Utility class for Nova file operations
     */
    class NOVA_CORE_API NovaFileOperations 
    {
    public:
        /**
         * Creates a directory if it doesn't exist
         * @param path The path to create
         * @return True if operation was successful or directory already exists
         */
        static bool CreateDirectory(const std::string& path);
        
        /**
         * Creates a directory structure with multiple nested levels
         * @param path The full path to create
         * @return True if operation was successful
         */
        static bool CreateDirectoryStructure(const std::string& path);
        
        /**
         * Delete a file
         * @param path Path to the file
         * @return True if operation was successful
         */
        static bool DeleteFile(const std::string& path);
        
        /**
         * Delete a directory and all its contents
         * @param path Path to the directory
         * @return True if operation was successful
         */
        static bool DeleteDirectory(const std::string& path);
        
        /**
         * Check if a file exists
         * @param path Path to the file
         * @return True if file exists
         */
        static bool FileExists(const std::string& path);
        
        /**
         * Check if a directory exists
         * @param path Path to the directory
         * @return True if directory exists
         */
        static bool DirectoryExists(const std::string& path);
        
        /**
         * Copy a file from source to destination
         * @param source Source file path
         * @param destination Destination file path
         * @return True if operation was successful
         */
        static bool CopyFile(const std::string& source, const std::string& destination);
        
        /**
         * Move a file from source to destination
         * @param source Source file path
         * @param destination Destination file path
         * @return True if operation was successful
         */
        static bool MoveFile(const std::string& source, const std::string& destination);
        
        /**
         * Read the contents of a text file
         * @param path Path to the file
         * @return File contents as string, empty if failed
         */
        static std::string ReadTextFile(const std::string& path);
        
        /**
         * Write content to a text file
         * @param path Path to the file
         * @param content Content to write
         * @param append Whether to append or overwrite
         * @return True if operation was successful
         */
        static bool WriteTextFile(const std::string& path, const std::string& content, bool append = false);
        
        /**
         * List all files in a directory
         * @param path Directory path
         * @param recursive Whether to include subdirectories
         * @return Vector of file paths
         */
        static std::vector<std::string> ListFiles(const std::string& path, bool recursive = false);
        
        /**
         * Lists all files in a directory with detailed information
         * @param path Directory path
         * @param recursive Whether to include subdirectories
         * @param filter Filter options to apply
         * @return Vector of FileInfo structs
         */
        static std::vector<FileInfo> ListFilesDetailed(const std::string& path, 
                                                     bool recursive = false,
                                                     const FileFilterOptions& filter = {});
        
        /**
         * Find files matching a pattern
         * @param path The base directory to search in
         * @param pattern The pattern to match (supports * and ? wildcards)
         * @param recursive Whether to search in subdirectories
         * @return Vector of matching file paths
         */
        static std::vector<std::string> FindFiles(const std::string& path, 
                                                const std::string& pattern, 
                                                bool recursive = false);
        
        /**
         * Get detailed information about a file
         * @param path Path to the file
         * @return FileInfo struct with file details, or nullopt if file not found/accessible
         */
        static std::optional<FileInfo> GetFileInfo(const std::string& path);
        
        /**
         * Check if a path matches a pattern
         * @param path Path to check
         * @param pattern Pattern to match against (supports * and ? wildcards)
         * @return True if path matches pattern
         */
        static bool MatchesPattern(const std::string& path, const std::string& pattern);
        
        /**
         * Watch a directory for changes
         * @param path Directory to watch
         * @param callback Function to call when changes are detected
         * @param intervalMs How often to check for changes (in milliseconds)
         * @param recursive Whether to watch subdirectories
         * @return An ID that can be used to stop watching
         */
        static int WatchDirectory(const std::string& path, 
                                std::function<void(const std::string&, const std::string&)> callback,
                                unsigned int intervalMs = 1000,
                                bool recursive = false);
        
        /**
         * Stop watching a directory
         * @param watchId The ID returned from WatchDirectory
         * @return True if successfully stopped watching
         */
        static bool StopWatching(int watchId);
        
        /**
         * Get the parent directory of a path
         * @param path The path to get the parent of
         * @return Path to the parent directory
         */
        static std::string GetParentDirectory(const std::string& path);
        
        /**
         * Extract the filename from a path
         * @param path Path to extract from
         * @param includeExtension Whether to include the file extension
         * @return Filename component
         */
        static std::string GetFileName(const std::string& path, bool includeExtension = true);
        
        /**
         * Get the file extension
         * @param path Path to extract from
         * @return File extension (including the dot)
         */
        static std::string GetFileExtension(const std::string& path);
        
        /**
         * Join path components
         * @param basePath The base path
         * @param relativePath The relative path to append
         * @return Combined path
         */
        static std::string JoinPaths(const std::string& basePath, const std::string& relativePath);
        
        /**
         * Get a temporary file path that is guaranteed to be unique
         * @param prefix Optional prefix for the filename
         * @param extension Optional extension for the file (include the dot)
         * @return Path to a unique temporary file
         */
        static std::string GetTempFilePath(const std::string& prefix = "nova_", const std::string& extension = "");
        
        /**
         * Get the relative path between two absolute paths
         * @param from The path to start from
         * @param to The path to end at
         * @return Relative path from 'from' to 'to'
         */
        static std::string GetRelativePath(const std::string& from, const std::string& to);
        
        /**
         * Normalizes a path, converting relative paths to absolute using current working directory
         * @param path Path to normalize
         * @return Normalized (absolute) path
         */
        static std::string NormalizePath(const std::string& path);
        
    private:
        // Structure to keep track of directory watchers
        struct DirectoryWatcher {
            std::string path;
            bool recursive;
            unsigned int intervalMs;
            std::function<void(const std::string&, const std::string&)> callback;
            std::map<std::string, std::filesystem::file_time_type> lastModified;
            bool active;
            std::thread watchThread;
        };
        
        // Map of active directory watchers
        static std::map<int, DirectoryWatcher> watchers;
        static std::mutex watchersMutex;
        static int nextWatcherId;
        
        // Helper function to convert a wildcard pattern to a regex
        static std::regex WildcardToRegex(const std::string& pattern);
        
        // Helper function for the directory watcher
        static void WatcherThreadFunc(int watcherId);
    };
}
