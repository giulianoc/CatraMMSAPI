
#pragma once

#include "JSONUtils.h"
#include "spdlog/spdlog.h"

class CatraMMSAPI
{
	struct UserProfile
	{
		int64_t userKey;
		bool ldapEnabled;
		std::string name;
		std::string email;
		std::string password;
		std::string country;
		std::string timezone;
		time_t creationDate;
		bool insolvent;
		time_t expirationDate;
		std::string creditCard_cardNumber;
		std::string creditCard_nameOnCard;
		time_t creditCard_expiryDate;
		std::string creditCard_securityCode;
	};
	struct Cost
	{
		std::string description;
		std::string type;		 // storage or encoder
		int64_t monthlyCost; // eur
		int64_t stepFactor;
		int64_t minAmount;
		int64_t maxAmount;

		// i prossimi campi sono usati dalla GUI
		int64_t currentAmount;
		int64_t newAmount;
	};
	struct WorkspaceDetails
	{
		int64_t workspaceKey;
		bool enabled;
		std::string name;
		std::string maxEncodingPriority;
		std::string encodingPeriod;
		int64_t maxIngestionsNumber;

		int64_t maxStorageInGB;
		int64_t currentCostForStorage;
		int64_t dedicatedEncoder_power_1;
		int64_t currentCostForDedicatedEncoder_power_1;
		int64_t dedicatedEncoder_power_2;
		int64_t currentCostForDedicatedEncoder_power_2;
		int64_t dedicatedEncoder_power_3;
		int64_t currentCostForDedicatedEncoder_power_3;
		int64_t CDN_type_1;
		int64_t currentCostForCDN_type_1;
		bool support_type_1;
		int64_t currentCostForSupport_type_1;

		int64_t workspaceOwnerUserKey; // filled only if user is admin
		std::string workspaceOwnerUserName; // filled only if user is admin

		int64_t usageInMB;
		time_t lastUsageInMBUpdate;

		std::string languageCode;
		std::string timezone;
		nlohmann::json preferences;
		nlohmann::json externalDeliveries;
		time_t creationDate;
		std::string apiKey;
		bool owner;
		bool defaultWorkspace;
		time_t expirationDate;
		bool admin;
		bool createRemoveWorkspace;
		bool ingestWorkflow;
		bool createProfiles;
		bool deliveryAuthorization;
		bool shareWorkspace;
		bool editMedia;
		bool editConfiguration;
		bool killEncoding;
		bool cancelIngestionJob;
		bool editEncodersPool;
		bool applicationRecorder;

		// this field is used by the GUI
		std::vector<Cost> dedicatedResources;
		int64_t currentTotalCost;
		int64_t newTotalCost;
		int64_t differenceBetweenCurrentAndNewForStorage;
		int64_t differenceBetweenCurrentAndNewForDedicatedEncoder_power_1;
		int64_t differenceBetweenCurrentAndNewForDedicatedEncoder_power_2;
		int64_t differenceBetweenCurrentAndNewForDedicatedEncoder_power_3;
		int64_t differenceBetweenCurrentAndNewForCDN_type_1;
		int64_t differenceBetweenCurrentAndNewForSupport_type_1;
	};

  public:
	struct IngestionResult
	{
		int64_t key;
		std::string label;
	};
	struct VideoBitRate
	{
		int32_t width;
		int32_t height;
		std::string forceOriginalAspectRatio; // decrease, increase
		bool pad;
		int32_t kBitRate;
		int32_t kMaxRate;
		int32_t kBufferSize;
	};
	struct EncodingProfileVideo
	{
		std::string codec;
		std::string profile;
		bool twoPasses;
		std::string otherOutputParameters;
		int64_t frameRate;
		int64_t keyFrameIntervalInSeconds;

		std::vector<VideoBitRate> videoBitRates;
	};
	struct EncodingProfileAudio
	{
		std::string codec;
		std::string otherOutputParameters;
		int16_t channelsNumber;
		int32_t sampleRate;
		std::vector<int32_t> kBitRates;
	};
	struct EncodingProfileImage
	{
		int32_t width;
		int32_t height;
		bool aspectRatio;
		int32_t maxWidth;
		int32_t maxHeight;
		std::string interlaceType;
	};
	struct EncodingProfile
	{
		int64_t encodingProfileKey;
		bool global;
		std::string label;
		std::string contentType;
		std::string fileFormat;
		std::string description;
		EncodingProfileVideo videoDetails;
		EncodingProfileAudio audioDetails;
		EncodingProfileImage imageDetails;
		nlohmann::json encodingProfileRoot;
	};
	struct Encoder
	{
		int64_t encoderKey;
		std::string label;
		bool external;
		bool enabled;
		std::string protocol;
		std::string publicServerName;
		std::string internalServerName;
		int32_t port;
		bool running;
		int32_t cpuUsage;
		nlohmann::json workspacesAssociatedRoot;
	};
	struct EncodersPool
	{
		int64_t encodersPoolKey;
		std::string label;
		std::vector<Encoder> encoders;
	};
	struct EncodingProfilesSet
	{
		int64_t encodingProfilesSetKey;
		std::string label;
		std::string contentType;

		std::vector<EncodingProfile> encodingProfiles;
	};
	struct RTMPChannelConf
	{
		int64_t confKey;
		std::string label;
		std::string rtmpURL;
		std::string streamName;
		std::string userName;
		std::string password;
		nlohmann::json playURLDetails;
		std::string type;
		long outputIndex;
		int64_t reservedByIngestionJobKey;
		std::string configurationLabel;
	};
	struct SRTChannelConf
	{
		int64_t confKey;
		std::string label;
		std::string srtURL;
		std::string mode;
		std::string streamId;
		std::string passphrase;
		std::string playURL;
		std::string type;
		long outputIndex;
		int64_t reservedByIngestionJobKey;
		std::string configurationLabel;
	};

	explicit CatraMMSAPI(nlohmann::json &configurationRoot);
	~CatraMMSAPI() = default;

	UserProfile userProfile;
	WorkspaceDetails currentWorkspaceDetails;
	std::string mmsVersion;

	std::vector<std::string> videoFileFormats;
	std::vector<std::string> audioFileFormats;
	std::vector<std::string> imageFileFormats;

	void login(std::string userName, std::string password, std::string clientIPAddress = "");
	std::vector<EncodingProfile> getEncodingProfiles(std::string contentType, int64_t encodingProfileKey = -1, std::string label = "", bool cacheAllowed = true);
	std::vector<EncodersPool> getEncodersPool(bool cacheAllowed = true);
	std::vector<EncodingProfilesSet> getEncodingProfilesSets(std::string contentType, bool cacheAllowed = true);
	std::vector<RTMPChannelConf> getRTMPChannelConf(std::string label = "", bool labelLike = true, std::string type = "", bool cacheAllowed = true);
	std::vector<SRTChannelConf> getSRTChannelConf(std::string label = "", bool labelLike = true, std::string type = "", bool cacheAllowed = true);
	std::pair<IngestionResult, std::vector<IngestionResult>> ingestionWorkflow(nlohmann::json workflowRoot);
	void ingestionBinary(int64_t addContentIngestionJobKey, std::string pathFileName, std::function<bool(int, int)> chunkCompleted);

  private:
	bool _loginSuccessful;
	std::string _userName;
	std::string _password;

	int32_t _apiTimeoutInSeconds;
	int32_t _apiMaxRetries;
	int32_t _statisticsTimeoutInSeconds;
	int32_t _deliveryMaxRetries;
	std::string _apiProtocol;
	std::string _apiHostname;
	int32_t _apiPort;
	std::string _binaryProtocol;
	std::string _binaryHostname;
	int32_t _binaryPort;
	int32_t _binaryTimeoutInSeconds;
	int32_t _binaryMaxRetries;
	bool _outputToBeCompressed;

	UserProfile fillUserProfile(nlohmann::json userProfileRoot);
	WorkspaceDetails fillWorkspaceDetails(nlohmann::json workspacedetailsRoot);
	EncodingProfile fillEncodingProfile(nlohmann::json encodingProfileRoot, bool deep);
	EncodingProfilesSet fillEncodingProfilesSet(nlohmann::json encodingProfilesSetRoot, bool deep);
	EncodersPool fillEncodersPool(nlohmann::json encodersPoolRoot);
	Encoder fillEncoder(nlohmann::json encoderRoot);
	RTMPChannelConf fillRTMPChannelConf(nlohmann::json rtmpChannelConfRoot);
	SRTChannelConf fillSRTChannelConf(nlohmann::json srtChannelConfRoot);
};
