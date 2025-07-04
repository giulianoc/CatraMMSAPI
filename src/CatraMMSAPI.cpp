
#include "CatraMMSAPI.h"
#include "CurlWrapper.h"
#include "Datetime.h"
#include <exception>
#include <format>
#include <optional>
#include <stdexcept>
#include <tuple>

CatraMMSAPI::CatraMMSAPI(json configurationRoot)
{
	_apiTimeoutInSeconds = JSONUtils::asInt(configurationRoot["mms"]["api"], "timeoutInSeconds", 15);
	SPDLOG_DEBUG(
		"Configuration item"
		", mms->timeoutInSeconds: {}",
		_apiTimeoutInSeconds
	);

	_apiMaxRetries = JSONUtils::asInt(configurationRoot["mms"]["api"], "maxRetries", 1);
	SPDLOG_DEBUG(
		"Configuration item"
		", mms->api->maxRetries: {}",
		_apiMaxRetries
	);

	_statisticsTimeoutInSeconds = JSONUtils::asInt(configurationRoot["mms"]["statistics"], "timeoutInSeconds", 30);
	SPDLOG_DEBUG(
		"Configuration item"
		", mms->statistics->timeoutInSeconds: {}",
		_statisticsTimeoutInSeconds
	);

	_deliveryMaxRetries = JSONUtils::asInt(configurationRoot["mms"]["delivery"], "maxRetriesNumber", 2);
	SPDLOG_DEBUG(
		"Configuration item"
		", mms->delivery->maxRetriesNumber: {}",
		_deliveryMaxRetries
	);

	_apiProtocol = JSONUtils::asString(configurationRoot["mms"]["api"], "protocol", "https");
	SPDLOG_DEBUG(
		"Configuration item"
		", mms->api->protocol: {}",
		_apiProtocol
	);

	_apiHostname = JSONUtils::asString(configurationRoot["mms"]["api"], "hostname", "mms-api.catramms-cloud.com");
	SPDLOG_DEBUG(
		"Configuration item"
		", mms->api->hostname: {}",
		_apiHostname
	);

	_apiPort = JSONUtils::asInt(configurationRoot["mms"]["api"], "port", 443);
	SPDLOG_DEBUG(
		"Configuration item"
		", mms->api->port: {}",
		_apiPort
	);

	_binaryProtocol = JSONUtils::asString(configurationRoot["mms"]["binary"], "protocol", "https");
	SPDLOG_DEBUG(
		"Configuration item"
		", mms->binary->protocol: {}",
		_binaryProtocol
	);

	_binaryHostname = JSONUtils::asString(configurationRoot["mms"]["binary"], "hostname", "mms-binary.catramms-cloud.com");
	SPDLOG_DEBUG(
		"Configuration item"
		", mms->binary->hostname: {}",
		_binaryHostname
	);

	_binaryPort = JSONUtils::asInt(configurationRoot["mms"]["binary"], "port", 80);
	SPDLOG_DEBUG(
		"Configuration item"
		", mms->binary->port: {}",
		_binaryPort
	);

	_binaryTimeoutInSeconds = JSONUtils::asInt(configurationRoot["mms"]["binary"], "timeoutInSeconds", 180);
	SPDLOG_DEBUG(
		"Configuration item"
		", mms->binary->timeoutInSeconds: {}",
		_binaryTimeoutInSeconds
	);

	_binaryMaxRetries = JSONUtils::asInt(configurationRoot["mms"]["binary"], "maxRetries", 1);
	SPDLOG_DEBUG(
		"Configuration item"
		", mms->binary->maxRetries: {}",
		_binaryMaxRetries
	);

	_outputToBeCompressed = JSONUtils::asBool(configurationRoot["mms"], "outputToBeCompressed", true);
	SPDLOG_DEBUG(
		"Configuration item"
		", mms->outputToBeCompressed: {}",
		_outputToBeCompressed
	);

	_loginSuccessful = false;

	{
		videoFileFormats.push_back("mp4");
		videoFileFormats.push_back("m4v");
		videoFileFormats.push_back("mkv");
		videoFileFormats.push_back("mov");
		videoFileFormats.push_back("ts");
		videoFileFormats.push_back("wmv");
		videoFileFormats.push_back("mpeg");
		videoFileFormats.push_back("mxf");
		videoFileFormats.push_back("mts");
		videoFileFormats.push_back("avi");
		videoFileFormats.push_back("webm");
		videoFileFormats.push_back("hls");
	}

	{
		audioFileFormats.push_back("mp3");
		audioFileFormats.push_back("aac");
		audioFileFormats.push_back("m4a");
		audioFileFormats.push_back("wav");
		audioFileFormats.push_back("hls");
	}

	{
		imageFileFormats.push_back("jpg");
		audioFileFormats.push_back("jpeg");
		imageFileFormats.push_back("png");
		imageFileFormats.push_back("gif");
		audioFileFormats.push_back("tif");
		imageFileFormats.push_back("tga");
	}
}

void CatraMMSAPI::login(string userName, string password, string clientIPAddress)
{
	if (clientIPAddress.empty())
	{
		try
		{
			string url = "https://api.ipify.org?format=json";
			SPDLOG_INFO(
				"httpGetJson"
				", url: {}"
				", apiTimeoutInSeconds: {}",
				url, _apiTimeoutInSeconds
			);
			json clientIPRoot = CurlWrapper::httpGetJson(url, _apiTimeoutInSeconds);
			clientIPAddress = JSONUtils::asString(clientIPRoot, "ip");
			SPDLOG_INFO(
				"httpGetJson"
				", url: {}"
				", clientIPAddress: {}",
				url, clientIPAddress
			);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"localIPAddresses failed"
				", exception: {}",
				e.what()
			);
			SPDLOG_ERROR(errorMessage);

			// throw;
		}
	}

	try
	{
		_loginSuccessful = false;

		string url = std::format("{}://{}:{}/catramms/1.0.1/login", _apiProtocol, _apiHostname, _apiPort);

		json bodyRoot;

		bodyRoot["email"] = userName;
		bodyRoot["password"] = password;
		if (!clientIPAddress.empty())
			bodyRoot["remoteClientIPAddress"] = clientIPAddress;

		SPDLOG_INFO(
			"httpPostStringAndGetJson"
			", url: {}"
			", body: {}",
			url, "..." // JSONUtils::toString(bodyRoot) commentato per evitare di mostrare la password
		);
		json mmsInfoRoot = CurlWrapper::httpPostStringAndGetJson(
			url, _apiTimeoutInSeconds, CurlWrapper::basicAuthorization(userName, password), JSONUtils::toString(bodyRoot)
		);

		userProfile = fillUserProfile(mmsInfoRoot);
		userProfile.password = password;

		if (!JSONUtils::isMetadataPresent(mmsInfoRoot, "workspace"))
		{
			string errorMessage = "No valid Workspace available for the User. Please contact the administrator"
								  ", userName: {}",
				   userName;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		currentWorkspaceDetails = fillWorkspaceDetails(mmsInfoRoot["workspace"]);

		mmsVersion = JSONUtils::asString(mmsInfoRoot, "mmsVersion");

		_userName = userName;
		_password = password;
		_loginSuccessful = true;
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"login failed"
			", userName: {}"
			", exception: {}",
			userName, e.what()
		);
		SPDLOG_ERROR(errorMessage);

		throw;
	}
}

pair<CatraMMSAPI::IngestionResult, vector<CatraMMSAPI::IngestionResult>> CatraMMSAPI::ingestionWorkflow(json workflowRoot)
{
	string api = "ingestionWorkflow";

	if (!_loginSuccessful)
	{
		string errorMessage = "login API was not called yet";
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	try
	{
		string url = std::format("{}://{}:{}/catramms/1.0.1/workflow", _apiProtocol, _apiHostname, _apiPort);

		SPDLOG_INFO(
			"httpGetJson"
			", url: {}"
			", _outputToBeCompressed: {}",
			url, _outputToBeCompressed
		);
		vector<string> otherHeaders;
		json mmsInfoRoot = CurlWrapper::httpPostStringAndGetJson(
			url, _apiTimeoutInSeconds, CurlWrapper::basicAuthorization(std::format("{}", userProfile.userKey), currentWorkspaceDetails.apiKey),
			JSONUtils::toString(workflowRoot), "application/json", vector<string>(), "", _apiMaxRetries, 15, false // _outputToBeCompressed
		);

		IngestionResult workflowResult;
		{
			json workflowRoot = JSONUtils::asJson(mmsInfoRoot, "workflow");

			workflowResult.key = JSONUtils::asInt64(workflowRoot, "ingestionRootKey", -1);
			workflowResult.label = JSONUtils::asString(workflowRoot, "label", "");
		}

		vector<IngestionResult> ingestionJobs;
		{
			json tasksRoot = JSONUtils::asJson(mmsInfoRoot, "tasks", json::array());
			for (auto &[keyRoot, valRoot] : tasksRoot.items())
			{
				IngestionResult ingestionJobResult;

				ingestionJobResult.key = JSONUtils::asInt64(valRoot, "ingestionJobKey", -1);
				ingestionJobResult.label = JSONUtils::asString(valRoot, "label", "");

				ingestionJobs.push_back(ingestionJobResult);
			}
		}

		return make_pair(workflowResult, ingestionJobs);
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"{} failed"
			", exception: {}",
			api, e.what()
		);
		SPDLOG_ERROR(errorMessage);

		throw;
	}
}

void CatraMMSAPI::ingestionBinary(int64_t addContentIngestionJobKey, string pathFileName, function<bool(int, int)> chunkCompleted)
{
	string api = "ingestionBinary";

	if (!_loginSuccessful)
	{
		string errorMessage = "login API was not called yet";
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	try
	{
		string url = std::format("{}://{}:{}/catramms/1.0.1/binary/{}", _binaryProtocol, _binaryHostname, _binaryPort, addContentIngestionJobKey);

		SPDLOG_INFO(
			"httpGetJson"
			", url: {}"
			", _outputToBeCompressed: {}",
			url, _outputToBeCompressed
		);

		string sResponse = CurlWrapper::httpPostFileSplittingInChunks(
			url, _binaryTimeoutInSeconds, CurlWrapper::basicAuthorization(std::format("{}", userProfile.userKey), currentWorkspaceDetails.apiKey),
			pathFileName, chunkCompleted, "", _binaryMaxRetries, _binaryTimeoutInSeconds
		);
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"{} failed"
			", exception: {}",
			api, e.what()
		);
		SPDLOG_ERROR(errorMessage);

		throw;
	}
}

vector<CatraMMSAPI::EncodingProfile> CatraMMSAPI::getEncodingProfiles(string contentType, int64_t encodingProfileKey, string label, bool cacheAllowed)
{
	string api = "getEncodingProfiles";

	if (!_loginSuccessful)
	{
		string errorMessage = "login API was not called yet";
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	try
	{
		string url = std::format("{}://{}:{}/catramms/1.0.1/encodingProfiles/{}", _apiProtocol, _apiHostname, _apiPort, contentType);
		if (encodingProfileKey != -1)
			url += std::format("/{}", encodingProfileKey);
		char queryChar = '?';
		if (!label.empty())
		{
			url += std::format("{}label={}", queryChar, CurlWrapper::escape(label));
			queryChar = '&';
		}
		url += std::format("{}should_bypass_cache={}", queryChar, cacheAllowed);

		SPDLOG_INFO(
			"httpGetJson"
			", url: {}"
			", _outputToBeCompressed: {}",
			url, _outputToBeCompressed
		);
		vector<string> otherHeaders;
		if (_outputToBeCompressed)
			otherHeaders.push_back("X-ResponseBodyCompressed: true");
		json mmsInfoRoot = CurlWrapper::httpGetJson(
			url, _apiTimeoutInSeconds, CurlWrapper::basicAuthorization(std::format("{}", userProfile.userKey), currentWorkspaceDetails.apiKey),
			otherHeaders, "", _apiMaxRetries, 15, _outputToBeCompressed
		);

		json responseRoot = JSONUtils::asJson(mmsInfoRoot, "response");
		json encodingProfilesRoot = JSONUtils::asJson(responseRoot, "encodingProfiles", json::array());

		vector<EncodingProfile> encodingProfiles;

		bool deep = false;
		for (auto &[keyRoot, valRoot] : encodingProfilesRoot.items())
			encodingProfiles.push_back(fillEncodingProfile(valRoot, deep));

		return encodingProfiles;
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"{} failed"
			", exception: {}",
			api, e.what()
		);
		SPDLOG_ERROR(errorMessage);

		throw;
	}
}

vector<CatraMMSAPI::EncodingProfilesSet> CatraMMSAPI::getEncodingProfilesSets(string contentType, bool cacheAllowed)
{
	string api = "getEncodingProfilesSets";

	if (!_loginSuccessful)
	{
		string errorMessage = "login API was not called yet";
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	try
	{
		string url = std::format("{}://{}:{}/catramms/1.0.1/encodingProfilesSets/{}", _apiProtocol, _apiHostname, _apiPort, contentType);
		char queryChar = '?';
		url += std::format("{}should_bypass_cache={}", queryChar, cacheAllowed);

		SPDLOG_INFO(
			"httpGetJson"
			", url: {}"
			", _outputToBeCompressed: {}",
			url, _outputToBeCompressed
		);
		vector<string> otherHeaders;
		if (_outputToBeCompressed)
			otherHeaders.push_back("X-ResponseBodyCompressed: true");
		json mmsInfoRoot = CurlWrapper::httpGetJson(
			url, _apiTimeoutInSeconds, CurlWrapper::basicAuthorization(std::format("{}", userProfile.userKey), currentWorkspaceDetails.apiKey),
			otherHeaders, "", _apiMaxRetries, 15, _outputToBeCompressed
		);

		json responseRoot = JSONUtils::asJson(mmsInfoRoot, "response");
		json encodingProfilesSetsRoot = JSONUtils::asJson(responseRoot, "encodingProfilesSets", json::array());

		vector<EncodingProfilesSet> encodingProfilesSets;

		bool deep = true;
		for (auto &[keyRoot, valRoot] : encodingProfilesSetsRoot.items())
			encodingProfilesSets.push_back(fillEncodingProfilesSet(valRoot, deep));

		return encodingProfilesSets;
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"{} failed"
			", exception: {}",
			api, e.what()
		);
		SPDLOG_ERROR(errorMessage);

		throw;
	}
}

vector<CatraMMSAPI::EncodersPool> CatraMMSAPI::getEncodersPool(bool cacheAllowed)
{
	string api = "getEncodersPool";

	if (!_loginSuccessful)
	{
		string errorMessage = "login API was not called yet";
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	try
	{
		string url = std::format("{}://{}:{}/catramms/1.0.1/encodersPool?labelOrder=asc", _apiProtocol, _apiHostname, _apiPort);
		char queryChar = '?';
		url += std::format("{}should_bypass_cache={}", queryChar, cacheAllowed);

		SPDLOG_INFO(
			"httpGetJson"
			", url: {}"
			", _outputToBeCompressed: {}",
			url, _outputToBeCompressed
		);
		vector<string> otherHeaders;
		if (_outputToBeCompressed)
			otherHeaders.push_back("X-ResponseBodyCompressed: true");
		json mmsInfoRoot = CurlWrapper::httpGetJson(
			url, _apiTimeoutInSeconds, CurlWrapper::basicAuthorization(std::format("{}", userProfile.userKey), currentWorkspaceDetails.apiKey),
			otherHeaders, "", _apiMaxRetries, 15, _outputToBeCompressed
		);

		json responseRoot = JSONUtils::asJson(mmsInfoRoot, "response");
		json encodersPoolRoot = JSONUtils::asJson(responseRoot, "encodersPool", json::array());

		vector<EncodersPool> encodersPool;

		for (auto &[keyRoot, valRoot] : encodersPoolRoot.items())
			encodersPool.push_back(fillEncodersPool(valRoot));

		return encodersPool;
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"{} failed"
			", exception: {}",
			api, e.what()
		);
		SPDLOG_ERROR(errorMessage);

		throw;
	}
}

CatraMMSAPI::UserProfile CatraMMSAPI::fillUserProfile(json userProfileRoot)
{
	try
	{
		CatraMMSAPI::UserProfile userProfile;

		userProfile.userKey = JSONUtils::asInt64(userProfileRoot, "userKey", -1);
		userProfile.ldapEnabled = JSONUtils::asBool(userProfileRoot, "ldapEnabled", false);
		userProfile.name = JSONUtils::asString(userProfileRoot, "name", "");
		userProfile.country = JSONUtils::asString(userProfileRoot, "country", "");
		userProfile.timezone = JSONUtils::asString(userProfileRoot, "timezone", "");
		userProfile.email = JSONUtils::asString(userProfileRoot, "email", "");
		userProfile.creationDate = Datetime::parseUtcStringToUtcInSecs(JSONUtils::asString(userProfileRoot, "creationDate", ""));
		userProfile.insolvent = JSONUtils::asBool(userProfileRoot, "insolvent", false);
		userProfile.expirationDate = Datetime::parseUtcStringToUtcInSecs(JSONUtils::asString(userProfileRoot, "expirationDate", ""));

		return userProfile;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"fillUserProfile failed"
			", exception: {}",
			e.what()
		);
		throw;
	}
}

CatraMMSAPI::WorkspaceDetails CatraMMSAPI::fillWorkspaceDetails(json workspacedetailsRoot)
{
	try
	{
		CatraMMSAPI::WorkspaceDetails workspaceDetails;

		workspaceDetails.workspaceKey = JSONUtils::asInt64(workspacedetailsRoot, "workspaceKey", -1);
		workspaceDetails.enabled = JSONUtils::asBool(workspacedetailsRoot, "enabled", false);
		workspaceDetails.name = JSONUtils::asString(workspacedetailsRoot, "workspaceName", "");
		workspaceDetails.maxEncodingPriority = JSONUtils::asString(workspacedetailsRoot, "maxEncodingPriority", "");
		workspaceDetails.encodingPeriod = JSONUtils::asString(workspacedetailsRoot, "encodingPeriod", "");
		workspaceDetails.maxIngestionsNumber = JSONUtils::asInt64(workspacedetailsRoot, "maxIngestionsNumber", -1);
		workspaceDetails.usageInMB = JSONUtils::asInt64(workspacedetailsRoot, "workSpaceUsageInMB", -1);
		workspaceDetails.languageCode = JSONUtils::asString(workspacedetailsRoot, "languageCode", "");
		workspaceDetails.timezone = JSONUtils::asString(workspacedetailsRoot, "timezone", "");

		workspaceDetails.preferences = nullptr;
		if (!JSONUtils::asString(workspacedetailsRoot, "preferences", "").empty())
		{
			try
			{
				workspaceDetails.preferences = JSONUtils::toJson(JSONUtils::asString(workspacedetailsRoot, "preferences", ""));
			}
			catch (exception &e)
			{
				SPDLOG_ERROR("Wrong workspaceDetails.preferences format: {}", JSONUtils::asString(workspacedetailsRoot, "preferences", ""));
			}
		}

		workspaceDetails.creationDate = Datetime::parseUtcStringToUtcInSecs(JSONUtils::asString(workspacedetailsRoot, "creationDate", ""));
		workspaceDetails.workspaceOwnerUserKey = JSONUtils::asInt64(workspacedetailsRoot, "workspaceOwnerUserKey", -1);
		workspaceDetails.workspaceOwnerUserName = JSONUtils::asString(workspacedetailsRoot, "workspaceOwnerUserName", "");
		if (JSONUtils::isMetadataPresent(workspacedetailsRoot, "userAPIKey"))
		{
			json userAPIKeyRoot = JSONUtils::asJson(workspacedetailsRoot, "userAPIKey");

			workspaceDetails.apiKey = JSONUtils::asString(userAPIKeyRoot, "apiKey", "");
			workspaceDetails.owner = JSONUtils::asBool(userAPIKeyRoot, "owner", false);
			workspaceDetails.defaultWorkspace = JSONUtils::asBool(userAPIKeyRoot, "default", false);
			workspaceDetails.expirationDate = Datetime::parseUtcStringToUtcInSecs(JSONUtils::asString(userAPIKeyRoot, "expirationDate", ""));
			workspaceDetails.admin = JSONUtils::asBool(userAPIKeyRoot, "admin", false);
			workspaceDetails.createRemoveWorkspace = JSONUtils::asBool(userAPIKeyRoot, "createRemoveWorkspace", false);
			workspaceDetails.ingestWorkflow = JSONUtils::asBool(userAPIKeyRoot, "ingestWorkflow", false);
			workspaceDetails.createProfiles = JSONUtils::asBool(userAPIKeyRoot, "createProfiles", false);
			workspaceDetails.deliveryAuthorization = JSONUtils::asBool(userAPIKeyRoot, "deliveryAuthorization", false);
			workspaceDetails.shareWorkspace = JSONUtils::asBool(userAPIKeyRoot, "shareWorkspace", false);
			workspaceDetails.editMedia = JSONUtils::asBool(userAPIKeyRoot, "editMedia", false);
			workspaceDetails.editConfiguration = JSONUtils::asBool(userAPIKeyRoot, "editConfiguration", false);
			workspaceDetails.killEncoding = JSONUtils::asBool(userAPIKeyRoot, "killEncoding", false);
			workspaceDetails.cancelIngestionJob = JSONUtils::asBool(userAPIKeyRoot, "cancelIngestionJob", false);
			workspaceDetails.editEncodersPool = JSONUtils::asBool(userAPIKeyRoot, "editEncodersPool", false);
			workspaceDetails.applicationRecorder = JSONUtils::asBool(userAPIKeyRoot, "applicationRecorder", false);
		}
		if (JSONUtils::isMetadataPresent(workspacedetailsRoot, "cost"))
		{
			json costInfoRoot = JSONUtils::asJson(workspacedetailsRoot, "cost");

			workspaceDetails.maxStorageInGB = JSONUtils::asInt64(costInfoRoot, "maxStorageInGB", -1);
			workspaceDetails.currentCostForStorage = JSONUtils::asInt64(costInfoRoot, "currentCostForStorage", -1);
			workspaceDetails.dedicatedEncoder_power_1 = JSONUtils::asInt64(costInfoRoot, "dedicatedEncoder_power_1", -1);
			workspaceDetails.currentCostForDedicatedEncoder_power_1 = JSONUtils::asInt64(costInfoRoot, "currentCostForDedicatedEncoder_power_1", -1);
			workspaceDetails.dedicatedEncoder_power_2 = JSONUtils::asInt64(costInfoRoot, "dedicatedEncoder_power_2", -1);
			workspaceDetails.currentCostForDedicatedEncoder_power_2 = JSONUtils::asInt64(costInfoRoot, "currentCostForDedicatedEncoder_power_2", -1);
			workspaceDetails.dedicatedEncoder_power_3 = JSONUtils::asInt64(costInfoRoot, "dedicatedEncoder_power_3", -1);
			workspaceDetails.currentCostForDedicatedEncoder_power_3 = JSONUtils::asInt64(costInfoRoot, "currentCostForDedicatedEncoder_power_3", -1);
			workspaceDetails.CDN_type_1 = JSONUtils::asInt64(costInfoRoot, "CDN_type_1", -1);
			workspaceDetails.currentCostForCDN_type_1 = JSONUtils::asInt64(costInfoRoot, "currentCostForCDN_type_1", -1);
			workspaceDetails.support_type_1 = JSONUtils::asBool(costInfoRoot, "support_type_1", false);
			workspaceDetails.currentCostForSupport_type_1 = JSONUtils::asInt64(costInfoRoot, "currentCostForSupport_type_1", -1);
		}

		return workspaceDetails;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"fillWorkspaceDetails failed"
			", exception: {}",
			e.what()
		);
		throw;
	}
}

CatraMMSAPI::EncodingProfile CatraMMSAPI::fillEncodingProfile(json encodingProfileRoot, bool deep)
{
	try
	{
		CatraMMSAPI::EncodingProfile encodingProfile;

		encodingProfile.global = JSONUtils::asBool(encodingProfileRoot, "global", false);
		encodingProfile.encodingProfileKey = JSONUtils::asInt64(encodingProfileRoot, "encodingProfileKey", -1);
		encodingProfile.label = JSONUtils::asString(encodingProfileRoot, "label", "");
		encodingProfile.contentType = JSONUtils::asString(encodingProfileRoot, "contentType", "");

		json profileInfoRoot = JSONUtils::asJson(encodingProfileRoot, "profile");
		encodingProfile.fileFormat = JSONUtils::asString(profileInfoRoot, "fileFormat", "");
		if (deep)
		{
			if (encodingProfile.contentType == "video")
			{
				json videoInfoRoot = JSONUtils::asJson(profileInfoRoot, "video");
				encodingProfile.videoDetails.codec = JSONUtils::asString(videoInfoRoot, "codec", "");
				encodingProfile.videoDetails.profile = JSONUtils::asString(videoInfoRoot, "profile", "");
				encodingProfile.videoDetails.twoPasses = JSONUtils::asBool(videoInfoRoot, "twoPasses", false);
				encodingProfile.videoDetails.otherOutputParameters = JSONUtils::asString(videoInfoRoot, "otherOutputParameters", "");
				encodingProfile.videoDetails.frameRate = JSONUtils::asInt(videoInfoRoot, "frameRate", -1);
				encodingProfile.videoDetails.keyFrameIntervalInSeconds = JSONUtils::asInt(videoInfoRoot, "keyFrameIntervalInSeconds", -1);
				{
					json bitRatesRoot = JSONUtils::asJson(videoInfoRoot, "bitRates", json::array());
					for (auto &[keyRoot, valRoot] : bitRatesRoot.items())
					{
						VideoBitRate videoBitRate;

						videoBitRate.width = JSONUtils::asInt(valRoot, "width", -1);
						videoBitRate.height = JSONUtils::asInt(valRoot, "height", -1);
						videoBitRate.kBitRate = JSONUtils::asInt(valRoot, "kBitRate", -1);
						videoBitRate.forceOriginalAspectRatio = JSONUtils::asInt(valRoot, "forceOriginalAspectRatio", -1);
						videoBitRate.pad = JSONUtils::asInt(valRoot, "pad", -1);
						videoBitRate.kMaxRate = JSONUtils::asInt(valRoot, "kMaxRate", -1);
						videoBitRate.kBufferSize = JSONUtils::asInt(valRoot, "kBufferSize", -1);

						encodingProfile.videoDetails.videoBitRates.push_back(videoBitRate);
					}
				}

				json audioInfoRoot = JSONUtils::asJson(profileInfoRoot, "audio");
				encodingProfile.audioDetails.codec = JSONUtils::asString(audioInfoRoot, "codec", "");
				encodingProfile.audioDetails.otherOutputParameters = JSONUtils::asString(audioInfoRoot, "otherOutputParameters", "");
				encodingProfile.audioDetails.channelsNumber = JSONUtils::asInt(audioInfoRoot, "channelsNumber", -1);
				encodingProfile.audioDetails.sampleRate = JSONUtils::asInt(audioInfoRoot, "sampleRate", -1);
				{
					json bitRatesRoot = JSONUtils::asJson(audioInfoRoot, "bitRates", json::array());
					for (auto &[keyRoot, valRoot] : bitRatesRoot.items())
						encodingProfile.audioDetails.kBitRates.push_back(JSONUtils::asInt(valRoot, "kBitRate", -1));
				}
			}
			else if (encodingProfile.contentType == "audio")
			{
				json audioInfoRoot = JSONUtils::asJson(profileInfoRoot, "audio");
				encodingProfile.audioDetails.codec = JSONUtils::asString(audioInfoRoot, "codec", "");
				encodingProfile.audioDetails.otherOutputParameters = JSONUtils::asString(audioInfoRoot, "otherOutputParameters", "");
				encodingProfile.audioDetails.channelsNumber = JSONUtils::asInt(audioInfoRoot, "channelsNumber", -1);
				encodingProfile.audioDetails.sampleRate = JSONUtils::asInt(audioInfoRoot, "sampleRate", -1);
				{
					json bitRatesRoot = JSONUtils::asJson(audioInfoRoot, "bitRates", json::array());
					for (auto &[keyRoot, valRoot] : bitRatesRoot.items())
						encodingProfile.audioDetails.kBitRates.push_back(JSONUtils::asInt(valRoot, "kBitRate", -1));
				}
			}
			else if (encodingProfile.contentType == "image")
			{
				json imageInfoRoot = JSONUtils::asJson(profileInfoRoot, "image");
				encodingProfile.imageDetails.width = JSONUtils::asInt(imageInfoRoot, "width", -1);
				encodingProfile.imageDetails.height = JSONUtils::asInt(imageInfoRoot, "height", -1);
				encodingProfile.imageDetails.aspectRatio = JSONUtils::asBool(imageInfoRoot, "aspectRatio", -1);
				encodingProfile.imageDetails.maxWidth = JSONUtils::asInt(imageInfoRoot, "maxWidth", -1);
				encodingProfile.imageDetails.maxHeight = JSONUtils::asInt(imageInfoRoot, "maxHeight", -1);
				encodingProfile.imageDetails.interlaceType = JSONUtils::asString(imageInfoRoot, "interlaceType", "");
			}
		}

		return encodingProfile;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"fillEncodingProfile failed"
			", exception: {}",
			e.what()
		);
		throw;
	}
}

CatraMMSAPI::EncodingProfilesSet CatraMMSAPI::fillEncodingProfilesSet(json encodingProfilesSetRoot, bool deep)
{
	try
	{
		CatraMMSAPI::EncodingProfilesSet encodingProfilesSet;

		encodingProfilesSet.encodingProfilesSetKey = JSONUtils::asInt64(encodingProfilesSetRoot, "encodingProfilesSetKey", -1);
		encodingProfilesSet.contentType = JSONUtils::asString(encodingProfilesSetRoot, "contentType", "");
		encodingProfilesSet.label = JSONUtils::asString(encodingProfilesSetRoot, "label", "");
		if (deep)
		{
			json encodingProfilesRoot = JSONUtils::asJson(encodingProfilesSetRoot, "encodingProfiles", json::array());
			for (auto &[keyRoot, valRoot] : encodingProfilesRoot.items())
				encodingProfilesSet.encodingProfiles.push_back(fillEncodingProfile(valRoot, deep));
		}

		return encodingProfilesSet;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"fillEncodingProfilesSet failed"
			", exception: {}",
			e.what()
		);
		throw;
	}
}

CatraMMSAPI::EncodersPool CatraMMSAPI::fillEncodersPool(json encodersPoolRoot)
{
	try
	{
		CatraMMSAPI::EncodersPool encodersPool;

		encodersPool.encodersPoolKey = JSONUtils::asInt64(encodersPoolRoot, "encodersPoolKey", -1);
		encodersPool.label = JSONUtils::asString(encodersPoolRoot, "label", "");
		{
			json encodersRoot = JSONUtils::asJson(encodersPoolRoot, "encoders", json::array());
			for (auto &[keyRoot, valRoot] : encodersRoot.items())
				encodersPool.encoders.push_back(fillEncoder(valRoot));
		}

		return encodersPool;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"fillEncodersPool failed"
			", exception: {}",
			e.what()
		);
		throw;
	}
}

CatraMMSAPI::Encoder CatraMMSAPI::fillEncoder(json encoderRoot)
{
	try
	{
		CatraMMSAPI::Encoder encoder;

		encoder.encoderKey = JSONUtils::asInt64(encoderRoot, "encoderKey", -1);
		encoder.label = JSONUtils::asString(encoderRoot, "label", "");
		encoder.external = JSONUtils::asBool(encoderRoot, "external", false);
		encoder.enabled = JSONUtils::asBool(encoderRoot, "enabled", false);
		encoder.protocol = JSONUtils::asString(encoderRoot, "protocol", "");
		encoder.publicServerName = JSONUtils::asString(encoderRoot, "publicServerName", "");
		encoder.internalServerName = JSONUtils::asString(encoderRoot, "internalServerName", "");
		encoder.port = JSONUtils::asInt(encoderRoot, "port", -1);
		encoder.running = JSONUtils::asBool(encoderRoot, "running", false);
		encoder.cpuUsage = JSONUtils::asInt(encoderRoot, "cpuUsage", -1);
		encoder.workspacesAssociatedRoot = JSONUtils::asJson(encoderRoot, "workspacesAssociated", json::array());

		return encoder;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"fillEncoder failed"
			", exception: {}",
			e.what()
		);
		throw;
	}
}
