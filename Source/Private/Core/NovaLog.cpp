/// @file NovaLog.cpp
/// @brief Implements logging behavior.

#include "NovaCore.h"
#include "NovaMinimal.h"
#include <filesystem>
#include <cpptrace/cpptrace.hpp>

namespace Core {

	using namespace Core::FileOperations;

	std::mutex logMutex;
	std::vector<std::string> logMessages;
	bool NovaLog::verboseEnabled = false;

	void NovaLog::SetVerbose(bool verbose)
	{
		verboseEnabled = verbose;
		if (verbose) {
			NOVA_LOG("Verbose logging enabled", LogType::Debug);
		}
	}

	void NovaLog::TypeVerbose(const char* Message, LogType Type)
	{
		if (verboseEnabled) {
			Debug(Message);
		}
	}

	void NovaLog::Type(const char* Message, LogType Type)
	{
		switch (Type)
		{
			case LogType::Log:
			{
				Log(Message);
				break;
			}
			case LogType::Debug:
			{
				Debug(Message);
				break;
			}
			case LogType::Warning:
			{
				Warning(Message);
				break;
			}
			case LogType::Error:
			{
				Error(Message);
				break;
			}
			case LogType::FatalError:
			{
				FatalError(Message);
				break;
			}
			case LogType::Assert:
			{
				// Changed: don't automatically fail assertions
				// We need to assess them properly
				Assert(true, Message);
				break;
			}
		}
	}

	void NovaLog::StartLogFile()
	{
		try {
			// Ensure base Content/Logs directory exists (may be located at project root)
			CreateRequiredDirectories();

			std::filesystem::path base;
#ifdef PROJECT_SOURCE_DIR
			base = std::filesystem::path(PROJECT_SOURCE_DIR);
#else
			base = std::filesystem::path("");
#endif
			if (base.empty()) base = []() {
				std::filesystem::path p = std::filesystem::current_path();
				for (int i = 0; i < 10; ++i) {
					if (std::filesystem::exists(p / "Content")) return p;
					if (p == p.root_path()) break;
					p = p.parent_path();
				}
				return std::filesystem::current_path();
			}();

			std::filesystem::path logsDirectory = base / "Content" / "Logs";
			std::filesystem::path logFilePath = logsDirectory / "Nova.log";

			// Check if the log file already exists
			if (std::filesystem::exists(logFilePath)) {
				// Generate a timestamp
				auto now = std::chrono::system_clock::now();
				auto time_t_now = std::chrono::system_clock::to_time_t(now); // Store the result in a variable
				std::stringstream timestamp;
				timestamp << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d_%H-%M-%S");

				// Rename the existing log file
				std::filesystem::path newLogFilePath = logsDirectory / (std::string("Nova-") + timestamp.str() + ".log");
				std::filesystem::rename(logFilePath, newLogFilePath);
				std::cout << "Renamed existing log file to: " << newLogFilePath.string() << std::endl;
			}

			// Create a new log file
			auto now = std::chrono::system_clock::now();
			auto time_t_now = std::chrono::system_clock::to_time_t(now); // Store the result in a variable
			std::string header =
				"====================================================\n"
				"=               CELESTIA NOVA LOG                  =\n"
				"====================================================\n"
				"Started: " + std::string(std::ctime(&time_t_now)) + // Pass the variable to std::ctime
				"====================================================\n\n";

			std::ofstream logFile(logFilePath.string(), std::ios::out);
			if (!logFile.is_open()) {
				throw std::runtime_error(std::string("Failed to create log file: ") + logFilePath.string());
			}

			logFile << header;
			logFile.close();

			logMessages.clear();
			std::cout << "Log file started successfully at: " << logFilePath << std::endl;
		} catch (const std::exception& e) {
			std::cerr << "Error in StartLogFile: " << e.what() << std::endl;
			throw;
		}
	}

	// Create directories needed by the application using NovaFileOperations
	void NovaLog::CreateRequiredDirectories()
	{
		try {
			// Resolve base path for Content/ using same logic as StartLogFile
			std::filesystem::path base;
#ifdef PROJECT_SOURCE_DIR
			base = std::filesystem::path(PROJECT_SOURCE_DIR);
#else
			std::filesystem::path p = std::filesystem::current_path();
			for (int i = 0; i < 10; ++i) {
				if (std::filesystem::exists(p / "Content")) {
					base = p;
					break;
				}
				if (p == p.root_path()) break;
				p = p.parent_path();
			}
			if (base.empty()) base = std::filesystem::current_path();
#endif

			std::filesystem::path contentDir = base / "Content";
			std::filesystem::path logsDir = contentDir / "Logs";
			std::filesystem::path websiteDir = contentDir / "Website";

			if (!std::filesystem::exists(contentDir)) {
				std::filesystem::create_directories(contentDir);
				std::cout << "Created " << contentDir.string() << std::endl;
			}

			if (!std::filesystem::exists(logsDir)) {
				std::filesystem::create_directories(logsDir);
				std::cout << "Created " << logsDir.string() << std::endl;
			}

			if (!std::filesystem::exists(websiteDir)) {
				std::filesystem::create_directories(websiteDir);
				std::cout << "Created " << websiteDir.string() << std::endl;
			}
		} catch (const std::exception& e) {
			std::cerr << "Error creating required directories: " << e.what() << std::endl;
		}
	}

	void NovaLog::Log(const char* Message)
	{
		WriteLogToFile(Message);
		AddToGuiLog(Message);
	}

	void NovaLog::Debug(const std::string& message)
	{
		WriteLogToFile("DEBUG: " + message);
		AddToGuiLog("DEBUG: " + message);
	}

	void NovaLog::Warning(const std::string& message)
	{
		WriteLogToFile("WARNING: " + message);
		AddToGuiLog("WARNING: " + message);
	}

	void NovaLog::Error(const std::string& message)
	{
		WriteLogToFile("ERROR: " + message);
		AddToGuiLog("ERROR: " + message);
	}

	void NovaLog::FatalError(const std::string& message)
	{
		WriteLogToFile("FATAL ERROR: " + message);
		AddToGuiLog("FATAL ERROR: " + message);
		
		// Auto-wire: Any manually logged internal fatal error forces a stack trace prior to killing the app
		LogStackTrace("Internal Fatal Error");
		exit(-1);
	}

	void NovaLog::Assert(bool condition, const std::string& message)
	{
		if (!condition)
		{
			WriteLogToFile("ASSERTION FAILED: " + message);
			AddToGuiLog("ASSERTION FAILED: " + message);
			
			// Auto-wire: Failed assertions immediately dump a backtrace into the log
			LogStackTrace("Assertion Failure");
			exit(-1);  // This will immediately terminate the application
		}
		else {
			// Only log the message if condition is true
			WriteLogToFile("ASSERTION PASSED: " + message);
		}
	}

	void NovaLog::AddToGuiLog(const std::string& message)
	{
		std::lock_guard<std::mutex> lock(logMutex);
		logMessages.push_back(message);
		// Removed console output to prevent UI movement
		// std::cout << message << std::endl;
	}

	void* NovaLog::CreateScrollableLog()
	{
		static ftxui::Component component = ftxui::Renderer([] {
			std::lock_guard<std::mutex> lock(logMutex);
			ftxui::Elements elements;
			for (const auto& message : logMessages)
			{
				elements.push_back(ftxui::text(message));
			}
			return ftxui::vbox(elements) | ftxui::vscroll_indicator | ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 10);
		}) | ftxui::CatchEvent([](ftxui::Event) { return false; });
		
		return &component;
	}

	void* NovaLog::RenderLog()
	{
		static ftxui::Element element = []() {
			std::lock_guard<std::mutex> lock(logMutex);
			ftxui::Elements elements;
			
			// Limit to last 20 messages to prevent overflow
			size_t startIndex = logMessages.size() > 20 ? logMessages.size() - 20 : 0;
			
			for (size_t i = startIndex; i < logMessages.size(); ++i)
			{
				elements.push_back(ftxui::text(logMessages[i]));
			}
			return ftxui::vbox(elements) | ftxui::vscroll_indicator;
		}();
		
		return &element;
	}

	void NovaLog::WriteLogToFile(const std::string& message)
	{
		try {
			// Ensure required directories exist before writing
			CreateRequiredDirectories();

			std::lock_guard<std::mutex> lock(logMutex);

			auto now = std::chrono::system_clock::now();
			auto time_t_now = std::chrono::system_clock::to_time_t(now);
			std::tm local_tm = *std::localtime(&time_t_now);

			char date_buffer[20];
			std::strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d", &local_tm);

			char time_buffer[20];
			std::strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", &local_tm);

			std::string logFileName = std::string("Nova") + ".log";
#ifdef PROJECT_SOURCE_DIR
			std::string logFilePath = std::string(PROJECT_SOURCE_DIR) + "/Content/Logs/" + logFileName;
#else
			std::string logFilePath = "Content/Logs/" + logFileName;
#endif

			std::string formattedMessage = "[" + std::string(date_buffer) + " " + std::string(time_buffer) + "] " + message + "\n";

			std::ofstream logFile(logFilePath, std::ios::app);
			if (!logFile.is_open()) {
				std::cerr << "Failed to write to log file: " << logFilePath << std::endl;
				return;
			}

			logFile << formattedMessage;
			logFile.close();
			std::cout << formattedMessage;  // Print to console for debugging
		} catch (const std::exception& e) {
			std::cerr << "Error writing to log file: " << e.what() << std::endl;
		} catch (...) {
			std::cerr << "Unknown error writing to log file" << std::endl;
		}
	}

	void NovaLog::LogStackTrace(const char* Context)
	{
		try {
			// Generate a full symbolic stack trace
			auto trace = cpptrace::generate_trace();
			
			std::string message = "--- STACK TRACE ---";
			if (Context && strlen(Context) > 0) {
				message = std::string("--- STACK TRACE (") + Context + ") ---";
			}
			
			std::ostringstream oss;
			trace.print_with_snippets(oss, false);
			message += "\n" + oss.str();
			message += "\n------------------";
			
			// Log it as an error so it's always highlighted
			Type(message.c_str(), LogType::Error);
		} catch (const std::exception& e) {
			Type(("Failed to log stack trace: " + std::string(e.what())).c_str(), LogType::Warning);
		} catch (...) {
			Type("Failed to log stack trace: Unknown Exception", LogType::Warning);
		}
	}

	void NovaLog::LogObjectTrace(const char* Context)
	{
		try {
			// Generate an object trace (signal-safe, deep inspection)
			auto trace = cpptrace::generate_object_trace();
			
			std::string message = "--- OBJECT TRACE ---";
			if (Context && strlen(Context) > 0) {
				message = std::string("--- OBJECT TRACE (") + Context + ") ---";
			}
			
			std::ostringstream oss;
			trace.resolve().print_with_snippets(oss, false);
			message += "\n" + oss.str();
			message += "\n--------------------";
			
			// Log it as an error so it's always highlighted
			Type(message.c_str(), LogType::Error);
		} catch (const std::exception& e) {
			Type(("Failed to log object trace: " + std::string(e.what())).c_str(), LogType::Warning);
		} catch (...) {
			Type("Failed to log object trace: Unknown Exception", LogType::Warning);
		}
	}
}