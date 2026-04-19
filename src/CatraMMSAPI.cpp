
#include "CatraMMSAPI.h"
#include "CurlWrapper.h"
#include "Datetime.h"
#include "JsonPath.h"

#include <exception>
#include <format>
#include <optional>
#include <stdexcept>
#include <tuple>

using namespace std;
using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;

CatraMMSAPI::CatraMMSAPI(json &configurationRoot) : userProfile(), currentWorkspaceDetails()
{
	_apiTimeoutInSeconds = JsonPath(&configurationRoot)["mms"]["api"]["timeoutInSeconds"].as<int32_t>(15);
	LOG_DEBUG(
		"Configuration item"
		", mms->timeoutInSeconds: {}",
		_apiTimeoutInSeconds
	);

	_apiMaxRetries = JsonPath(&configurationRoot)["mms"]["api"]["maxRetries"].as<int32_t>(1);
	LOG_DEBUG(
		"Configuration item"
		", mms->api->maxRetries: {}",
		_apiMaxRetries
	);

	_statisticsTimeoutInSeconds = JsonPath(&configurationRoot)["mms"]["statistics"]["timeoutInSeconds"].as<int32_t>(30);
	LOG_DEBUG(
		"Configuration item"
		", mms->statistics->timeoutInSeconds: {}",
		_statisticsTimeoutInSeconds
	);

	_deliveryMaxRetries = JsonPath(&configurationRoot)["mms"]["delivery"]["maxRetriesNumber"].as<int32_t>(2);
	LOG_DEBUG(
		"Configuration item"
		", mms->delivery->maxRetriesNumber: {}",
		_deliveryMaxRetries
	);

	_apiProtocol = JsonPath(&configurationRoot)["mms"]["api"]["protocol"].as<string>("https");
	LOG_DEBUG(
		"Configuration item"
		", mms->api->protocol: {}",
		_apiProtocol
	);

	_apiHostname = JsonPath(&configurationRoot)["mms"]["api"]["hostname"].as<string>("mms-api.catramms-cloud.com");
	LOG_DEBUG(
		"Configuration item"
		", mms->api->hostname: {}",
		_apiHostname
	);

	_apiPort = JsonPath(&configurationRoot)["mms"]["api"]["port"].as<int32_t>(443);
	LOG_DEBUG(
		"Configuration item"
		", mms->api->port: {}",
		_apiPort
	);

	_binaryProtocol = JsonPath(&configurationRoot)["mms"]["binary"]["protocol"].as<string>("https");
	LOG_DEBUG(
		"Configuration item"
		", mms->binary->protocol: {}",
		_binaryProtocol
	);

	_binaryHostname = JsonPath(&configurationRoot)["mms"]["binary"]["v"].as<string>("mms-binary.catramms-cloud.com");
	LOG_DEBUG(
		"Configuration item"
		", mms->binary->hostname: {}",
		_binaryHostname
	);

	_binaryPort = JsonPath(&configurationRoot)["mms"]["binary"]["port"].as<int32_t>(443);
	LOG_DEBUG(
		"Configuration item"
		", mms->binary->port: {}",
		_binaryPort
	);

	_binaryTimeoutInSeconds = JsonPath(&configurationRoot)["mms"]["binary"]["timeoutInSeconds"].as<int32_t>(180);
	LOG_DEBUG(
		"Configuration item"
		", mms->binary->timeoutInSeconds: {}",
		_binaryTimeoutInSeconds
	);

	_binaryMaxRetries = JsonPath(&configurationRoot)["mms"]["binary"]["maxRetries"].as<int32_t>(1);
	LOG_DEBUG(
		"Configuration item"
		", mms->binary->maxRetries: {}",
		_binaryMaxRetries
	);

	_outputToBeCompressed = JsonPath(&configurationRoot)["mms"]["outputToBeCompressed"].as<bool>(true);
	LOG_DEBUG(
		"Configuration item"
		", mms->outputToBeCompressed: {}",
		_outputToBeCompressed
	);

	_loginSuccessful = false;

	{
		videoFileFormats.emplace_back("mp4");
		videoFileFormats.emplace_back("m4v");
		videoFileFormats.emplace_back("mkv");
		videoFileFormats.emplace_back("mov");
		videoFileFormats.emplace_back("ts");
		videoFileFormats.emplace_back("wmv");
		videoFileFormats.emplace_back("mpeg");
		videoFileFormats.emplace_back("mxf");
		videoFileFormats.emplace_back("mts");
		videoFileFormats.emplace_back("avi");
		videoFileFormats.emplace_back("webm");
		videoFileFormats.emplace_back("hls");
	}

	{
		audioFileFormats.emplace_back("mp3");
		audioFileFormats.emplace_back("aac");
		audioFileFormats.emplace_back("m4a");
		audioFileFormats.emplace_back("wav");
		audioFileFormats.emplace_back("hls");
	}

	{
		imageFileFormats.emplace_back("jpg");
		audioFileFormats.emplace_back("jpeg");
		imageFileFormats.emplace_back("png");
		imageFileFormats.emplace_back("gif");
		audioFileFormats.emplace_back("tif");
		imageFileFormats.emplace_back("tga");
	}
}

void CatraMMSAPI::login(string userName, string password, string clientIPAddress)
{
	if (clientIPAddress.empty())
	{
		try
		{
			string url = "https://api.ipify.org?format=json";
			LOG_INFO(
				"httpGetJson"
				", url: {}"
				", apiTimeoutInSeconds: {}",
				url, _apiTimeoutInSeconds
			);
			json clientIPRoot = CurlWrapper::httpGetJson(url, _apiTimeoutInSeconds);
			clientIPAddress = JsonPath(&clientIPRoot)["ip"].as<string>();
			LOG_INFO(
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

		LOG_INFO(
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

		if (!JSONUtils::isPresent(mmsInfoRoot, "workspace"))
		{
			string errorMessage = "No valid Workspace available for the User. Please contact the administrator"
								  ", userName: {}",
				   userName;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		currentWorkspaceDetails = fillWorkspaceDetails(mmsInfoRoot["workspace"]);

		mmsVersion = JsonPath(&mmsInfoRoot)["mmsVersion"].as<string>("");

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

		LOG_INFO(
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
			auto workflowRoot = JsonPath(&mmsInfoRoot)["workflow"].as<json>();

			workflowResult.key = JsonPath(&workflowRoot)["ingestionRootKey"].as<int64_t>(-1);
			workflowResult.label = JsonPath(&workflowRoot)["label"].as<string>("");
		}

		vector<IngestionResult> ingestionJobs;
		{
			json tasksRoot = JsonPath(&mmsInfoRoot)["tasks"].as<json>(json::array());
			for (auto &[keyRoot, valRoot] : tasksRoot.items())
			{
				IngestionResult ingestionJobResult;

				ingestionJobResult.key = JsonPath(&valRoot)["ingestionJobKey"].as<int64_t>(-1);
				ingestionJobResult.label = JsonPath(&valRoot)["label"].as<string>();

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

void CatraMMSAPI::ingestionBinary(int64_t addContentIngestionJobKey, const string& pathFileName, function<bool(int, int)> chunkCompleted)
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
		string url = std::format("{}://{}:{}/catramms/1.0.1/binary/{}", _binaryProtocol, _binaryHostname, _binaryPort,
			addContentIngestionJobKey);

		LOG_INFO(
			"httpGetJson"
			", url: {}"
			", _outputToBeCompressed: {}",
			url, _outputToBeCompressed
		);

		string sResponse = CurlWrapper::httpPostFileSplittingInChunks(
			url, _binaryTimeoutInSeconds, CurlWrapper::basicAuthorization(std::format("{}", userProfile.userKey),
			currentWorkspaceDetails.apiKey), pathFileName, chunkCompleted, "", _binaryMaxRetries,
			_binaryTimeoutInSeconds
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

		LOG_INFO(
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

		json responseRoot = JsonPath(&mmsInfoRoot)["response"].as<json>();
		json encodingProfilesRoot = JsonPath(&responseRoot)["encodingProfiles"].as<json>(json::array());

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

		LOG_INFO(
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

		json responseRoot = JsonPath(&mmsInfoRoot)["response"].as<json>();
		json encodingProfilesSetsRoot = JsonPath(&responseRoot)["encodingProfilesSets"].as<json>(json::array());

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

		LOG_INFO(
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

		json responseRoot = JsonPath(&mmsInfoRoot)["response"].as<json>();
		json encodersPoolRoot = JsonPath(&responseRoot)["encodersPool"].as<json>(json::array());

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

vector<CatraMMSAPI::RTMPChannelConf> CatraMMSAPI::getRTMPChannelConf(string label, bool labelLike, string type, bool cacheAllowed)
{
	string api = "getRTMPChannelConf";

	if (!_loginSuccessful)
	{
		string errorMessage = "login API was not called yet";
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	try
	{
		string url = std::format("{}://{}:{}/catramms/1.0.1/conf/cdn/rtmp/channel", _apiProtocol, _apiHostname, _apiPort);
		char queryChar = '?';
		if (!label.empty())
		{
			url += std::format("{}label={}", queryChar, CurlWrapper::escape(label));
			queryChar = '&';
		}
		url += std::format("{}labelLike={}", queryChar, labelLike);
		queryChar = '&';
		if (!type.empty())
			url += std::format("{}type={}", queryChar, CurlWrapper::escape(label));
		url += std::format("{}should_bypass_cache={}", queryChar, cacheAllowed);

		LOG_INFO(
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

		json responseRoot = JsonPath(&mmsInfoRoot)["response"].as<json>();
		json rtmpChannelConfRoot = JsonPath(&responseRoot)["rtmpChannelConf"].as<json>(json::array());

		vector<RTMPChannelConf> rtmpChannelConfs;

		for (auto &[keyRoot, valRoot] : rtmpChannelConfRoot.items())
			rtmpChannelConfs.push_back(fillRTMPChannelConf(valRoot));

		return rtmpChannelConfs;
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

vector<CatraMMSAPI::SRTChannelConf> CatraMMSAPI::getSRTChannelConf(const string& label, bool labelLike, const string& type, bool cacheAllowed)
{
	string api = "getSRTChannelConf";

	if (!_loginSuccessful)
	{
		string errorMessage = "login API was not called yet";
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	try
	{
		string url = std::format("{}://{}:{}/catramms/1.0.1/conf/cdn/srt/channel", _apiProtocol, _apiHostname, _apiPort);
		char queryChar = '?';
		if (!label.empty())
		{
			url += std::format("{}label={}", queryChar, CurlWrapper::escape(label));
			queryChar = '&';
		}
		url += std::format("{}labelLike={}", queryChar, labelLike);
		queryChar = '&';
		if (!type.empty())
			url += std::format("{}type={}", queryChar, CurlWrapper::escape(label));
		url += std::format("{}should_bypass_cache={}", queryChar, cacheAllowed);

		LOG_INFO(
			"httpGetJson"
			", url: {}"
			", _outputToBeCompressed: {}",
			url, _outputToBeCompressed
		);
		vector<string> otherHeaders;
		if (_outputToBeCompressed)
			otherHeaders.emplace_back("X-ResponseBodyCompressed: true");
		json mmsInfoRoot = CurlWrapper::httpGetJson(
			url, _apiTimeoutInSeconds, CurlWrapper::basicAuthorization(std::format("{}", userProfile.userKey), currentWorkspaceDetails.apiKey),
			otherHeaders, "", _apiMaxRetries, 15, _outputToBeCompressed
		);

		json responseRoot = JsonPath(&mmsInfoRoot)["response"].as<json>();
		json srtChannelConfRoot = JsonPath(&responseRoot)["srtChannelConf"].as<json>(json::array());

		vector<SRTChannelConf> srtChannelConfs;

		for (auto &[keyRoot, valRoot] : srtChannelConfRoot.items())
			srtChannelConfs.push_back(fillSRTChannelConf(valRoot));

		return srtChannelConfs;
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

pair<vector<CatraMMSAPI::Stream>, int16_t> CatraMMSAPI::getStreams(
	optional<int32_t> startIndex, optional<int32_t> pageSize,
	optional<int64_t> confKey,
	optional<string> label, optional<bool> labelLike,
	optional<string>url,
	optional<string> sourceType, optional<string> type,
	optional<string> name,
	optional<string> region, optional<string> country,
	const string& labelOrder, bool cacheAllowed)
{
	string api = "getStream";

	if (!_loginSuccessful)
	{
		string errorMessage = "login API was not called yet";
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	try
	{
		string apiUrl = std::format("{}://{}:{}/catramms/1.0.1/conf/stream", _apiProtocol, _apiHostname, _apiPort);
		if (confKey)
			apiUrl += std::format("/{}", *confKey);
		char queryChar = '?';
		if (startIndex)
		{
			apiUrl += std::format("{}start={}", queryChar, *startIndex);
			queryChar = '&';
		}
		if (pageSize)
		{
			apiUrl += std::format("{}rows={}", queryChar, *pageSize);
			queryChar = '&';
		}
		if (label)
		{
			apiUrl += std::format("{}label={}", queryChar, CurlWrapper::escape(*label));
			queryChar = '&';
		}
		if (labelLike)
		{
			apiUrl += std::format("{}labelLike={}", queryChar, *labelLike);
			queryChar = '&';
		}
		if (url)
		{
			apiUrl += std::format("{}url={}", queryChar, CurlWrapper::escape(*url));
			queryChar = '&';
		}
		if (sourceType)
		{
			apiUrl += std::format("{}sourceType={}", queryChar, *sourceType);
			queryChar = '&';
		}
		if (type)
		{
			apiUrl += std::format("{}type={}", queryChar, CurlWrapper::escape(*type));
			queryChar = '&';
		}
		if (name)
		{
			apiUrl += std::format("{}name={}", queryChar, CurlWrapper::escape(*name));
			queryChar = '&';
		}
		if (region)
		{
			apiUrl += std::format("{}region={}", queryChar, CurlWrapper::escape(*region));
			queryChar = '&';
		}
		if (country)
		{
			apiUrl += std::format("{}country={}", queryChar, CurlWrapper::escape(*country));
			queryChar = '&';
		}
		{
			apiUrl += std::format("{}labelOrder={}", queryChar, labelOrder);
			queryChar = '&';
		}
		apiUrl += std::format("{}should_bypass_cache={}", queryChar, cacheAllowed);

		LOG_INFO(
			"httpGetJson"
			", apiUrl: {}"
			", _outputToBeCompressed: {}",
			apiUrl, _outputToBeCompressed
		);
		vector<string> otherHeaders;
		if (_outputToBeCompressed)
			otherHeaders.emplace_back("X-ResponseBodyCompressed: true");
		json mmsInfoRoot = CurlWrapper::httpGetJson(
			apiUrl, _apiTimeoutInSeconds, CurlWrapper::basicAuthorization(std::format("{}", userProfile.userKey), currentWorkspaceDetails.apiKey),
			otherHeaders, "", _apiMaxRetries, 15, _outputToBeCompressed
		);

		json responseRoot = JsonPath(&mmsInfoRoot)["response"].as<json>();
		auto numFound = JsonPath(&responseRoot)["numFound"].as<int16_t>();
		auto streamsRoot = JsonPath(&responseRoot)["streams"].as<json>(json::array());

		vector<Stream> streams;

		for (auto &[keyRoot, valRoot] : streamsRoot.items())
			streams.push_back(fillStream(valRoot));

		return make_pair(streams, numFound);
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

CatraMMSAPI::UserProfile CatraMMSAPI::fillUserProfile(const json& userProfileRoot)
{
	try
	{
		UserProfile userProfile;

		userProfile.userKey = JsonPath(&userProfileRoot)["userKey"].as<int64_t>(-1);
		userProfile.ldapEnabled = JsonPath(&userProfileRoot)["ldapEnabled"].as<bool>(false);
		userProfile.name = JsonPath(&userProfileRoot)["name"].as<string>();
		userProfile.country = JsonPath(&userProfileRoot)["country"].as<string>();
		userProfile.timezone = JsonPath(&userProfileRoot)["timezone"].as<string>();
		userProfile.email = JsonPath(&userProfileRoot)["email"].as<string>();
		userProfile.creationDate = Datetime::parseStringToUtcInSecs(JsonPath(&userProfileRoot)["creationDate"].as<string>());
		userProfile.insolvent = JsonPath(&userProfileRoot)["insolvent"].as<bool>(false);
		userProfile.expirationDate = Datetime::parseStringToUtcInSecs(JsonPath(&userProfileRoot)["expirationDate"].as<string>());

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

CatraMMSAPI::WorkspaceDetails CatraMMSAPI::fillWorkspaceDetails(const json& workspacedetailsRoot)
{
	try
	{
		WorkspaceDetails workspaceDetails;

		workspaceDetails.workspaceKey = JsonPath(&workspacedetailsRoot)["workspaceKey"].as<int64_t>(-1);
		workspaceDetails.enabled = JsonPath(&workspacedetailsRoot)["enabled"].as<bool>(false);
		workspaceDetails.name = JsonPath(&workspacedetailsRoot)["workspaceName"].as<string>();
		workspaceDetails.maxEncodingPriority = JsonPath(&workspacedetailsRoot)["maxEncodingPriority"].as<string>();
		workspaceDetails.encodingPeriod = JsonPath(&workspacedetailsRoot)["encodingPeriod"].as<string>();
		workspaceDetails.maxIngestionsNumber = JsonPath(&workspacedetailsRoot)["maxIngestionsNumber"].as<int64_t>(-1);
		workspaceDetails.usageInMB = JsonPath(&workspacedetailsRoot)["workSpaceUsageInMB"].as<int64_t>(-1);
		workspaceDetails.languageCode = JsonPath(&workspacedetailsRoot)["languageCode"].as<string>();
		workspaceDetails.timezone = JsonPath(&workspacedetailsRoot)["timezone"].as<string>();

		workspaceDetails.preferences = nullptr;
		if (!JsonPath(&workspacedetailsRoot)["preferences"].as<string>("").empty())
		{
			try
			{
				workspaceDetails.preferences = JSONUtils::toJson<json>(JsonPath(&workspacedetailsRoot)["preferences"].as<string>());
			}
			catch (exception &e)
			{
				SPDLOG_ERROR("Wrong workspaceDetails.preferences format: {}", JsonPath(&workspacedetailsRoot)["preferences"].as<string>());
			}
		}

		workspaceDetails.creationDate = Datetime::parseStringToUtcInSecs(JsonPath(&workspacedetailsRoot)["creationDate"].as<string>());
		workspaceDetails.workspaceOwnerUserKey = JsonPath(&workspacedetailsRoot)["workspaceOwnerUserKey"].as<int64_t>(-1);
		workspaceDetails.workspaceOwnerUserName = JsonPath(&workspacedetailsRoot)["workspaceOwnerUserName"].as<string>();
		if (JSONUtils::isPresent(workspacedetailsRoot, "userAPIKey"))
		{
			json userAPIKeyRoot = JsonPath(&workspacedetailsRoot)["userAPIKey"].as<json>();

			workspaceDetails.apiKey = JsonPath(&userAPIKeyRoot)["apiKey"].as<string>();
			workspaceDetails.owner = JsonPath(&userAPIKeyRoot)["owner"].as<bool>(false);
			workspaceDetails.defaultWorkspace = JsonPath(&userAPIKeyRoot)["default"].as<bool>(false);
			workspaceDetails.expirationDate = Datetime::parseStringToUtcInSecs(JsonPath(&userAPIKeyRoot)["expirationDate"].as<string>());
			workspaceDetails.admin = JsonPath(&userAPIKeyRoot)["admin"].as<bool>(false);
			workspaceDetails.createRemoveWorkspace = JsonPath(&userAPIKeyRoot)["createRemoveWorkspace"].as<bool>(false);
			workspaceDetails.ingestWorkflow = JsonPath(&userAPIKeyRoot)["ingestWorkflow"].as<bool>(false);
			workspaceDetails.createProfiles = JsonPath(&userAPIKeyRoot)["createProfiles"].as<bool>(false);
			workspaceDetails.deliveryAuthorization = JsonPath(&userAPIKeyRoot)["deliveryAuthorization"].as<bool>(false);
			workspaceDetails.shareWorkspace = JsonPath(&userAPIKeyRoot)["shareWorkspace"].as<bool>(false);
			workspaceDetails.editMedia = JsonPath(&userAPIKeyRoot)["editMedia"].as<bool>(false);
			workspaceDetails.editConfiguration = JsonPath(&userAPIKeyRoot)["editConfiguration"].as<bool>(false);
			workspaceDetails.killEncoding = JsonPath(&userAPIKeyRoot)["killEncoding"].as<bool>(false);
			workspaceDetails.cancelIngestionJob = JsonPath(&userAPIKeyRoot)["cancelIngestionJob"].as<bool>(false);
			workspaceDetails.editEncodersPool = JsonPath(&userAPIKeyRoot)["editEncodersPool"].as<bool>(false);
			workspaceDetails.applicationRecorder = JsonPath(&userAPIKeyRoot)["applicationRecorder"].as<bool>(false);
			workspaceDetails.appUploadMediaContent = JsonPath(&userAPIKeyRoot)["appUploadMediaContent"].as<bool>(false);
			workspaceDetails.appCaptureScreenAndProxy = JsonPath(&userAPIKeyRoot)["appCaptureScreenAndProxy"].as<bool>(false);
			workspaceDetails.appStreamAndProxy = JsonPath(&userAPIKeyRoot)["appStreamAndProxy"].as<bool>(false);
		}
		if (JSONUtils::isPresent(workspacedetailsRoot, "cost"))
		{
			json costInfoRoot = JsonPath(&workspacedetailsRoot)["cost"].as<json>();

			workspaceDetails.maxStorageInGB = JsonPath(&costInfoRoot)["maxStorageInGB"].as<int64_t>(-1);
			workspaceDetails.currentCostForStorage = JsonPath(&costInfoRoot)["currentCostForStorage"].as<int64_t>(-1);
			workspaceDetails.dedicatedEncoder_power_1 = JsonPath(&costInfoRoot)["dedicatedEncoder_power_1"].as<int64_t>(-1);
			workspaceDetails.currentCostForDedicatedEncoder_power_1 = JsonPath(&costInfoRoot)["currentCostForDedicatedEncoder_power_1"].as<int64_t>(-1);
			workspaceDetails.dedicatedEncoder_power_2 = JsonPath(&costInfoRoot)["dedicatedEncoder_power_2"].as<int64_t>(-1);
			workspaceDetails.currentCostForDedicatedEncoder_power_2 = JsonPath(&costInfoRoot)["currentCostForDedicatedEncoder_power_2"].as<int64_t>(-1);
			workspaceDetails.dedicatedEncoder_power_3 = JsonPath(&costInfoRoot)["dedicatedEncoder_power_3"].as<int64_t>(-1);
			workspaceDetails.currentCostForDedicatedEncoder_power_3 = JsonPath(&costInfoRoot)["currentCostForDedicatedEncoder_power_3"].as<int64_t>(-1);
			workspaceDetails.CDN_type_1 = JsonPath(&costInfoRoot)["CDN_type_1"].as<int64_t>(-1);
			workspaceDetails.currentCostForCDN_type_1 = JsonPath(&costInfoRoot)["currentCostForCDN_type_1"].as<int64_t>(-1);
			workspaceDetails.support_type_1 = JsonPath(&costInfoRoot)["support_type_1"].as<bool>(false);
			workspaceDetails.currentCostForSupport_type_1 = JsonPath(&costInfoRoot)["currentCostForSupport_type_1"].as<int64_t>(-1);
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

CatraMMSAPI::EncodingProfile CatraMMSAPI::fillEncodingProfile(const json& encodingProfileRoot, const bool deep)
{
	try
	{
		EncodingProfile encodingProfile;

		encodingProfile.global = JsonPath(&encodingProfileRoot)["global"].as<bool>(false);
		encodingProfile.encodingProfileKey = JsonPath(&encodingProfileRoot)["encodingProfileKey"].as<int64_t>(-1);
		encodingProfile.label = JsonPath(&encodingProfileRoot)["label"].as<string>();
		encodingProfile.contentType = JsonPath(&encodingProfileRoot)["contentType"].as<string>();

		encodingProfile.encodingProfileRoot = JsonPath(&encodingProfileRoot)["profile"].as<json>();
		encodingProfile.fileFormat = JsonPath(&encodingProfile.encodingProfileRoot)["fileFormat"].as<string>();
		encodingProfile.description = JsonPath(&encodingProfile.encodingProfileRoot)["description"].as<string>();
		if (deep)
		{
			if (encodingProfile.contentType == "video")
			{
				json videoInfoRoot = JsonPath(&encodingProfile.encodingProfileRoot)["video"].as<json>();
				encodingProfile.videoDetails.codec = JsonPath(&videoInfoRoot)["codec"].as<string>();
				encodingProfile.videoDetails.profile = JsonPath(&videoInfoRoot)["profile"].as<string>();
				encodingProfile.videoDetails.twoPasses = JsonPath(&videoInfoRoot)["twoPasses"].as<bool>(false);
				encodingProfile.videoDetails.otherOutputParameters = JsonPath(&videoInfoRoot)["otherOutputParameters"].as<string>();
				encodingProfile.videoDetails.frameRate = JsonPath(&videoInfoRoot)["frameRate"].as<int32_t>(-1);
				encodingProfile.videoDetails.keyFrameIntervalInSeconds = JsonPath(&videoInfoRoot)["keyFrameIntervalInSeconds"].as<int32_t>(-1);
				{
					json bitRatesRoot = JsonPath(&videoInfoRoot)["bitRates"].as<json>(json::array());
					for (auto &[keyRoot, valRoot] : bitRatesRoot.items())
					{
						VideoBitRate videoBitRate;

						videoBitRate.width = JsonPath(&valRoot)["width"].as<int32_t>(-1);
						videoBitRate.height = JsonPath(&valRoot)["height"].as<int32_t>(-1);
						videoBitRate.kBitRate = JsonPath(&valRoot)["kBitRate"].as<int32_t>(-1);
						videoBitRate.forceOriginalAspectRatio = JsonPath(&valRoot)["forceOriginalAspectRatio"].as<int32_t>(-1);
						videoBitRate.pad = JsonPath(&valRoot)["pad"].as<int32_t>(-1);
						videoBitRate.kMaxRate = JsonPath(&valRoot)["kMaxRate"].as<int32_t>(-1);
						videoBitRate.kBufferSize = JsonPath(&valRoot)["kBufferSize"].as<int32_t>(-1);

						encodingProfile.videoDetails.videoBitRates.push_back(videoBitRate);
					}
				}

				json audioInfoRoot = JsonPath(&encodingProfile.encodingProfileRoot)["audio"].as<json>();
				encodingProfile.audioDetails.codec = JsonPath(&audioInfoRoot)["codec"].as<string>();
				encodingProfile.audioDetails.otherOutputParameters = JsonPath(&audioInfoRoot)["otherOutputParameters"].as<string>();
				encodingProfile.audioDetails.channelsNumber = JsonPath(&audioInfoRoot)["channelsNumber"].as<int32_t>(-1);
				encodingProfile.audioDetails.sampleRate = JsonPath(&audioInfoRoot)["sampleRate"].as<int32_t>(-1);
				{
					json bitRatesRoot = JsonPath(&audioInfoRoot)["bitRates"].as<json>(json::array());
					for (auto &[keyRoot, valRoot] : bitRatesRoot.items())
						encodingProfile.audioDetails.kBitRates.push_back(JsonPath(&valRoot)["kBitRate"].as<int32_t>(-1));
				}
			}
			else if (encodingProfile.contentType == "audio")
			{
				json audioInfoRoot = JsonPath(&encodingProfile.encodingProfileRoot)["audio"].as<json>();
				encodingProfile.audioDetails.codec = JsonPath(&audioInfoRoot)["codec"].as<string>();
				encodingProfile.audioDetails.otherOutputParameters = JsonPath(&audioInfoRoot)["otherOutputParameters"].as<string>();
				encodingProfile.audioDetails.channelsNumber = JsonPath(&audioInfoRoot)["channelsNumber"].as<int32_t>(-1);
				encodingProfile.audioDetails.sampleRate = JsonPath(&audioInfoRoot)["sampleRate"].as<int32_t>(-1);
				{
					json bitRatesRoot = JsonPath(&audioInfoRoot)["bitRates"].as<json>(json::array());
					for (auto &[keyRoot, valRoot] : bitRatesRoot.items())
						encodingProfile.audioDetails.kBitRates.push_back(JsonPath(&valRoot)["kBitRate"].as<int32_t>(-1));
				}
			}
			else if (encodingProfile.contentType == "image")
			{
				json imageInfoRoot = JsonPath(&encodingProfile.encodingProfileRoot)["image"].as<json>();
				encodingProfile.imageDetails.width = JsonPath(&imageInfoRoot)["width"].as<int32_t>(-1);
				encodingProfile.imageDetails.height = JsonPath(&imageInfoRoot)["height"].as<int32_t>(-1);
				encodingProfile.imageDetails.aspectRatio = JsonPath(&imageInfoRoot)["aspectRatio"].as<bool>(false);
				encodingProfile.imageDetails.maxWidth = JsonPath(&imageInfoRoot)["maxWidth"].as<int32_t>(-1);
				encodingProfile.imageDetails.maxHeight = JsonPath(&imageInfoRoot)["maxHeight"].as<int32_t>(-1);
				encodingProfile.imageDetails.interlaceType = JsonPath(&imageInfoRoot)["interlaceType"].as<string>();
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

CatraMMSAPI::EncodingProfilesSet CatraMMSAPI::fillEncodingProfilesSet(const json& encodingProfilesSetRoot, const bool deep)
{
	try
	{
		EncodingProfilesSet encodingProfilesSet;

		encodingProfilesSet.encodingProfilesSetKey = JsonPath(&encodingProfilesSetRoot)["encodingProfilesSetKey"].as<int64_t>(-1);
		encodingProfilesSet.contentType = JsonPath(&encodingProfilesSetRoot)["contentType"].as<string>();
		encodingProfilesSet.label = JsonPath(&encodingProfilesSetRoot)["label"].as<string>();
		if (deep)
		{
			json encodingProfilesRoot = JsonPath(&encodingProfilesSetRoot)["encodingProfiles"].as<json>(json::array());
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

CatraMMSAPI::EncodersPool CatraMMSAPI::fillEncodersPool(const json& encodersPoolRoot)
{
	try
	{
		EncodersPool encodersPool;

		encodersPool.encodersPoolKey = JsonPath(&encodersPoolRoot)["encodersPoolKey"].as<int64_t>(-1);
		encodersPool.label = JsonPath(&encodersPoolRoot)["label"].as<string>();
		{
			json encodersRoot = JsonPath(&encodersPoolRoot)["encoders"].as<json>(json::array());
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

CatraMMSAPI::Encoder CatraMMSAPI::fillEncoder(const json& encoderRoot)
{
	try
	{
		Encoder encoder;

		encoder.encoderKey = JsonPath(&encoderRoot)["encoderKey"].as<int64_t>(-1);
		encoder.label = JsonPath(&encoderRoot)["label"].as<string>();
		encoder.external = JsonPath(&encoderRoot)["external"].as<bool>(false);
		encoder.enabled = JsonPath(&encoderRoot)["enabled"].as<bool>(false);
		encoder.protocol = JsonPath(&encoderRoot)["protocol"].as<string>();
		encoder.publicServerName = JsonPath(&encoderRoot)["publicServerName"].as<string>();
		encoder.internalServerName = JsonPath(&encoderRoot)["internalServerName"].as<string>();
		encoder.port = JsonPath(&encoderRoot)["port"].as<int32_t>(-1);
		encoder.running = JsonPath(&encoderRoot)["running"].as<bool>(false);
		encoder.cpuUsage = JsonPath(&encoderRoot)["cpuUsage"].as<int32_t>(-1);
		encoder.workspacesAssociatedRoot = JsonPath(&encoderRoot)["workspacesAssociated"].as<json>(json::array());

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

CatraMMSAPI::RTMPChannelConf CatraMMSAPI::fillRTMPChannelConf(json rtmpChannelConfRoot)
{
	try
	{
		RTMPChannelConf rtmpChannelConf;

		rtmpChannelConf.confKey = JsonPath(&rtmpChannelConfRoot)["confKey"].as<int64_t>(-1);
		rtmpChannelConf.label = JsonPath(&rtmpChannelConfRoot)["label"].as<string>();
		rtmpChannelConf.rtmpURL = JsonPath(&rtmpChannelConfRoot)["rtmpURL"].as<string>();
		rtmpChannelConf.streamName = JsonPath(&rtmpChannelConfRoot)["streamName"].as<string>();
		rtmpChannelConf.userName = JsonPath(&rtmpChannelConfRoot)["userName"].as<string>();
		rtmpChannelConf.password = JsonPath(&rtmpChannelConfRoot)["password"].as<string>();
		rtmpChannelConf.playURLDetails = JsonPath(&rtmpChannelConfRoot)["playURLDetails"].as<json>(json(nullptr));
		rtmpChannelConf.type = JsonPath(&rtmpChannelConfRoot)["type"].as<string>();
		rtmpChannelConf.outputIndex = JsonPath(&rtmpChannelConfRoot)["outputIndex"].as<int64_t>(-1);
		rtmpChannelConf.reservedByIngestionJobKey = JsonPath(&rtmpChannelConfRoot)["reservedByIngestionJobKey"].as<int64_t>(-1);
		rtmpChannelConf.configurationLabel = JsonPath(&rtmpChannelConfRoot)["configurationLabel"].as<string>();

		return rtmpChannelConf;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"fillRTMPChannelConf failed"
			", exception: {}",
			e.what()
		);
		throw;
	}
}

CatraMMSAPI::SRTChannelConf CatraMMSAPI::fillSRTChannelConf(const json& srtChannelConfRoot)
{
	try
	{
		SRTChannelConf srtChannelConf;

		srtChannelConf.confKey = JsonPath(&srtChannelConfRoot)["confKey"].as<int64_t>(-1);
		srtChannelConf.label = JsonPath(&srtChannelConfRoot)["label"].as<string>();
		srtChannelConf.srtURL = JsonPath(&srtChannelConfRoot)["srtURL"].as<string>();
		srtChannelConf.mode = JsonPath(&srtChannelConfRoot)["mode"].as<string>("caller");
		srtChannelConf.streamId = JsonPath(&srtChannelConfRoot)["streamId"].as<string>();
		srtChannelConf.passphrase = JsonPath(&srtChannelConfRoot)["passphrase"].as<string>();
		srtChannelConf.playURL = JsonPath(&srtChannelConfRoot)["playURL"].as<string>();
		srtChannelConf.type = JsonPath(&srtChannelConfRoot)["type"].as<string>();
		srtChannelConf.outputIndex = JsonPath(&srtChannelConfRoot)["outputIndex"].as<int64_t>(-1);
		srtChannelConf.reservedByIngestionJobKey = JsonPath(&srtChannelConfRoot)["reservedByIngestionJobKey"].as<int64_t>(-1);
		srtChannelConf.configurationLabel = JsonPath(&srtChannelConfRoot)["configurationLabel"].as<string>();

		return srtChannelConf;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"fillSRTChannelConf failed"
			", exception: {}",
			e.what()
		);
		throw;
	}
}

CatraMMSAPI::Stream CatraMMSAPI::fillStream(const json& streamRoot)
{
	try
	{
		Stream stream;

		stream.confKey = JsonPath(&streamRoot)["confKey"].as<int64_t>(-1);
		stream.label = JsonPath(&streamRoot)["label"].as<string>();
		stream.sourceType = JsonPath(&streamRoot)["sourceType"].as<string>();
		stream.encodersPoolKey = JsonPath(&streamRoot)["encodersPoolKey"].as<int64_t>(-1);
		stream.encodersPoolLabel = JsonPath(&streamRoot)["encodersPoolLabel"].as<string>();
		stream.url = JsonPath(&streamRoot)["url"].as<string>();
		stream.pushProtocol = JsonPath(&streamRoot)["pushProtocol"].as<string>();
		stream.pushEncoderKey = JsonPath(&streamRoot)["pushEncoderKey"].as<int64_t>(-1);
		stream.pushPublicEncoderName = JsonPath(&streamRoot)["pushPublicEncoderName"].as<bool>(false);
		stream.pushEncoderLabel = JsonPath(&streamRoot)["pushEncoderLabel"].as<string>();
		stream.pushEncoderName = JsonPath(&streamRoot)["pushEncoderName"].as<string>();
		stream.pushServerPort = JsonPath(&streamRoot)["pushServerPort"].as<int16_t>(-1);
		stream.pushURI = JsonPath(&streamRoot)["pushUri"].as<string>();
		stream.pushListenTimeout = JsonPath(&streamRoot)["pushListenTimeout"].as<int16_t>(-1);
		stream.captureLiveVideoDeviceNumber = JsonPath(&streamRoot)["captureLiveVideoDeviceNumber"].as<int16_t>(-1);
		stream.captureLiveVideoInputFormat = JsonPath(&streamRoot)["captureLiveVideoInputFormat"].as<string>();
		stream.captureLiveFrameRate = JsonPath(&streamRoot)["captureLiveFrameRate"].as<int16_t>(-1);
		stream.captureLiveWidth = JsonPath(&streamRoot)["captureLiveWidth"].as<int16_t>(-1);
		stream.captureLiveHeight = JsonPath(&streamRoot)["captureLiveHeight"].as<int16_t>(-1);
		stream.captureLiveAudioDeviceNumber = JsonPath(&streamRoot)["captureLiveAudioDeviceNumber"].as<int16_t>(-1);
		stream.captureLiveChannelsNumber = JsonPath(&streamRoot)["captureLiveChannelsNumber"].as<int16_t>(-1);
		stream.tvSourceTVConfKey = JsonPath(&streamRoot)["tvSourceTVConfKey"].as<int64_t>(-1);
		stream.type = JsonPath(&streamRoot)["type"].as<string>();
		stream.description = JsonPath(&streamRoot)["description"].as<string>();
		stream.name = JsonPath(&streamRoot)["name"].as<string>();
		stream.region = JsonPath(&streamRoot)["region"].as<string>();
		stream.country = JsonPath(&streamRoot)["country"].as<string>();
		stream.imageMediaItemKey = JsonPath(&streamRoot)["imageMediaItemKey"].as<int64_t>(-1);
		stream.imageUniqueName = JsonPath(&streamRoot)["imageUniqueName"].as<string>();
		stream.position = JsonPath(&streamRoot)["position"].as<int16_t>(-1);
		stream.userData = JsonPath(&streamRoot)["userData"].as<string>();

		return stream;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"fillStream failed"
			", exception: {}",
			e.what()
		);
		throw;
	}
}
