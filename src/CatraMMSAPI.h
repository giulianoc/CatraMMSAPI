
#ifndef CatraMMSAPI_h
#define CatraMMSAPI_h

#include "JSONUtils.h"
#include "spdlog/spdlog.h"

using namespace std;

using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;
using namespace nlohmann::literals;

class CatraMMSAPI
{
	struct UserProfile
	{
		int64_t userKey;
		bool ldapEnabled;
		string name;
		string email;
		string password;
		string country;
		string timezone;
		time_t creationDate;
		bool insolvent;
		time_t expirationDate;
		string creditCard_cardNumber;
		string creditCard_nameOnCard;
		time_t creditCard_expiryDate;
		string creditCard_securityCode;
	};
	struct Cost
	{

		string description;
		string type;		 // storage or encoder
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
		string name;
		string maxEncodingPriority;
		string encodingPeriod;
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
		string workspaceOwnerUserName; // filled only if user is admin

		int64_t usageInMB;
		time_t lastUsageInMBUpdate;

		string languageCode;
		string timezone;
		json preferences;
		json externalDeliveries;
		time_t creationDate;
		string apiKey;
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
		vector<Cost> dedicatedResources;
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
		string label;
	};
	struct VideoBitRate
	{
		int32_t width;
		int32_t height;
		string forceOriginalAspectRatio; // decrease, increase
		bool pad;
		int32_t kBitRate;
		int32_t kMaxRate;
		int32_t kBufferSize;
	};
	struct EncodingProfileVideo
	{
		string codec;
		string profile;
		bool twoPasses;
		string otherOutputParameters;
		int64_t frameRate;
		int64_t keyFrameIntervalInSeconds;

		vector<VideoBitRate> videoBitRates;
	};
	struct EncodingProfileAudio
	{
		string codec;
		string otherOutputParameters;
		int16_t channelsNumber;
		int32_t sampleRate;
		vector<int32_t> kBitRates;
	};
	struct EncodingProfileImage
	{
		int32_t width;
		int32_t height;
		bool aspectRatio;
		int32_t maxWidth;
		int32_t maxHeight;
		string interlaceType;
	};
	struct EncodingProfile
	{
		int64_t encodingProfileKey;
		bool global;
		string label;
		string contentType;
		string fileFormat;
		EncodingProfileVideo videoDetails;
		EncodingProfileAudio audioDetails;
		EncodingProfileImage imageDetails;
		json encodingProfileRoot;
	};
	struct Encoder
	{
		int64_t encoderKey;
		string label;
		bool external;
		bool enabled;
		string protocol;
		string publicServerName;
		string internalServerName;
		int32_t port;
		bool running;
		int32_t cpuUsage;
		json workspacesAssociatedRoot;
	};
	struct EncodersPool
	{
		int64_t encodersPoolKey;
		string label;
		vector<Encoder> encoders;
	};
	struct EncodingProfilesSet
	{
		int64_t encodingProfilesSetKey;
		string label;
		string contentType;

		vector<EncodingProfile> encodingProfiles;
	};
	struct RTMPChannelConf
	{
		int64_t confKey;
		string label;
		string rtmpURL;
		string streamName;
		string userName;
		string password;
		string playURL;
		string type;
		long outputIndex;
		int64_t reservedByIngestionJobKey;
		string configurationLabel;
	};

	CatraMMSAPI(json configurationRoot);
	~CatraMMSAPI() = default;

	UserProfile userProfile;
	WorkspaceDetails currentWorkspaceDetails;
	string mmsVersion;

	vector<string> videoFileFormats;
	vector<string> audioFileFormats;
	vector<string> imageFileFormats;

	void login(string userName, string password, string clientIPAddress = "");
	vector<EncodingProfile> getEncodingProfiles(string contentType, int64_t encodingProfileKey = -1, string label = "", bool cacheAllowed = true);
	vector<EncodersPool> getEncodersPool(bool cacheAllowed = true);
	vector<EncodingProfilesSet> getEncodingProfilesSets(string contentType, bool cacheAllowed = true);
	vector<RTMPChannelConf> getRTMPChannelConf(string label = "", bool labelLike = true, string type = "", bool cacheAllowed = true);
	pair<IngestionResult, vector<IngestionResult>> ingestionWorkflow(json workflowRoot);
	void ingestionBinary(int64_t addContentIngestionJobKey, string pathFileName, function<bool(int, int)> chunkCompleted);

  private:
	bool _loginSuccessful;
	string _userName;
	string _password;

	int32_t _apiTimeoutInSeconds;
	int32_t _apiMaxRetries;
	int32_t _statisticsTimeoutInSeconds;
	int32_t _deliveryMaxRetries;
	string _apiProtocol;
	string _apiHostname;
	int32_t _apiPort;
	string _binaryProtocol;
	string _binaryHostname;
	int32_t _binaryPort;
	int32_t _binaryTimeoutInSeconds;
	int32_t _binaryMaxRetries;
	bool _outputToBeCompressed;

	UserProfile fillUserProfile(json userProfileRoot);
	WorkspaceDetails fillWorkspaceDetails(json workspacedetailsRoot);
	EncodingProfile fillEncodingProfile(json encodingProfileRoot, bool deep);
	EncodingProfilesSet fillEncodingProfilesSet(json encodingProfilesSetRoot, bool deep);
	EncodersPool fillEncodersPool(json encodersPoolRoot);
	Encoder fillEncoder(json encoderRoot);
	RTMPChannelConf fillRTMPChannelConf(json rtmpChannelConfRoot);
};

#endif
