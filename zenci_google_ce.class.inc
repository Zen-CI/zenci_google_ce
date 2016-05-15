<?php
/**
 * @file
 * GoogleComputeEngine API wrapper class definition.
 */

// URL ZenCI GoogleComputeEngine API.
define('ZENCI_GOOGLE_OAUTH_TOKEN_URI', 'https://www.googleapis.com/oauth2/v4/token');
define('ZENCI_GOOGLE_CE_SCOPE', 'https://www.googleapis.com/auth/compute');
define('ZENCI_GOOGLE_CE_PROJECT_DOMAIN', 'appspot.gserviceaccount.com');
define('ZENCI_GOOGLE_CE_API_URI', 'https://www.googleapis.com/compute/v1/projects');
define('ZENCI_GOOGLE_CE_INSTANCE_SCOPE', 'https://www.googleapis.com/auth/cloud-platform');

use \Curl\Curl;

require_once 'libraries/php-curl-class/src/Curl/CaseInsensitiveArray.php';
require_once 'libraries/php-curl-class/src/Curl/Curl.php';


/**
 * GoogleComputeEngine API wrapper class.
 */
class GoogleComputeEngineAPI {

  /**
   * The Curl object used for performing HTTP requests.
   *
   * @var \Curl\Curl
   */
  protected $curl;

  /**
   * Token required to interact with ZenCI API.
   *
   * @var string
   */
  protected $token = '';

  /**
   * Token expire time.
   *
   * @var string
   */
  protected $token_expire = '';

  /**
   * Call back function to store token.
   *
   * @var string
   */
  protected $save_token_function = FALSE;

  /**
   * Organisation or User name.
   *
   * @var string
   */
  protected $owner_name;

  /**
   * Repository name.
   *
   * @var string
   */
  protected $repo_name;

  /**
   * Debug mode. If enabled, data will be printed via backdrop_set_message().
   *
   * @var bool
   */
  protected $debug = FALSE;

  /**
   * Latest error message from ZenCI API.
   *
   * @var string
   */
  protected $error_message = '';

  /**
   * Project ID.
   *
   * @var string
   */
  protected $project_id;

  /**
   * Private Key.
   *
   * @var string
   */
  protected $private_key;

  /**
   * SSH Public Key.
   *
   * @var string
   */
  protected $ssh_public_key;

  /**
   * Constructor.
   */
  public function __construct() {
    $this->reInitCurl();
    $this->setDebug();
  }

  /**
   * Initializate $this->curl with Curl class and preset headers and user agent.
   */
  public function reInitCurl() {
    $this->curl = new Curl();
    $this->curl->setHeader('Content-Type', 'application/json');
    $this->curl->setUserAgent('Backdrop CMS ZenCI GoogleComputeEngine API module');
    $this->curl->setHeader('Accept', '*/*');
  }

  /**
   * Set debug value. False by default.
   *
   * @param $debug boolean
   *   TRUE or FALSE
   */
  public function setDebug($debug = FALSE) {
    $this->debug = $debug;
  }

  /**
   * Set Project ID value. False by default.
   *
   * @param $project_id
   *   Project ID to retrive TOKEN for GoogleComputeEngine API.
   */
  public function setProjectID($project_id) {
    $this->project_id = $project_id;
  }

  /**
   * Set PrivateKey value. False by default.
   *
   * @param $private_key
   *   PrivateKey to retrive TOKEN for GoogleComputeEngine API.
   */
  public function setPrivateKey($private_key) {
    $this->private_key = $private_key;
  }

  /**
   * Set SSHPublicKey value. False by default.
   *
   * @param $ssh_public_key
   *   PublicKey to set auth ssh key.
   */
  public function setSSHPublicKey($ssh_public_key) {
    $this->ssh_public_key = $ssh_public_key;
  }
  /**
   * Set token for GoogleComputeEngine API.
   *
   * @param $token
   *   Token to access GoogleComputeEngine API.
   * @param $expire_timestamp
   *   Expiration Timestamp.
   */
  public function setToken($token, $expire) {
    $this->token = $token;
    $this->token_expire = $expire;
    $this->callSaveTokenCallback();
  }

  /**
   * Set Callback function to store token.
   *
   * @param $function
   *   Callback function
   */
  public function setSaveTokenFunction($function) {
    $this->save_token_function = $function;
  }

  /**
   * Get store token.
   *
   * @return
   *   A string value. Stored token for GoogleComputeEngine API access.
   */
  public function getToken() {

    if(empty($this->token) or $this->token_expire < time() + ZENCI_GOOGLE_CE_TOKEN_RENEWAL ) {
      // Time to get new token.
      $this->updateToken();
    }
    return $this->token;
  }

  /**
   * Call callback function to store Token.
   */
  private function callSaveTokenCallback() {
    if($this->save_token_function){
      $function = $this->save_token_function;
      $function($this->token, $this->token_expire);
    }
  }

  /**
   * Update Token.
   */
  public function updateToken() {
    $header = array(
      'alg' => 'RS256',
      'typ' => 'JWT',
    );

    $header_json = json_encode($header);

    $time = time();

    $claim = array(
      'iss' => $this->project_id . '@' . ZENCI_GOOGLE_CE_PROJECT_DOMAIN,
      'scope' => ZENCI_GOOGLE_CE_SCOPE,
      'aud' => ZENCI_GOOGLE_OAUTH_TOKEN_URI,
      'exp' => $time + 3600,
      'iat' => $time,
    );

    $claim_json = json_encode($claim);

    $request = base64_encode($header_json) . '.' . base64_encode($claim_json);
    $signature_base_encoded = $this->signRequest($request);

    $assertion = $request . '.' . $signature_base_encoded;

    $params = array(
      'grant_type' => 'urn:ietf:params:oauth:grant-type:jwt-bearer',
      'assertion' => $request . '.' . $signature_base_encoded,
    );

    $curl = new Curl();
    $curl->setHeader('Content-Type', 'application/x-www-form-urlencoded');
    $curl->setUserAgent('Backdrop CMS ZenCI GoogleComputeEngine API module');
    $curl->setHeader('Accept', '*/*');

    $curl->post(ZENCI_GOOGLE_OAUTH_TOKEN_URI, $params);

    if ($this->isError()) {
      throw new Exception('GoogleComputeEngineAPI: ' . $this->curl->curlErrorCode . ' ' . $this->curl->response->message);
    }
    else{
      $this->setToken($curl->response->access_token, $time + $curl->response->expires_in);
    }
  }

  /**
   * sign Request for Token reguest.
   */
  private function signRequest($reguest) {
    $signature = null;
    openssl_sign($reguest, $signature, $this->private_key, "SHA256");
    $signature = rtrim(strtr(base64_encode($signature), '+/', '-_'), '=');
    return $signature;
  }

  /**
   * Determine if curl request has been falen with error.
   *
   * @return boolean
   *   TRUE or FALSE based on answer from GoogleComputeEngine API.
   */
  public function isError() {
    return $this->curl->curlError or $this->curl->error;
  }

  /**
   * Get Curl details after request.
   *
   * @return array
   *   An array of request information:
   *     - code: the last error number. @see curl_errno.
   *     - message: A clear text error message for the last cURL operation.
   *   @see curl_error.
   *     - request_headers: an array of request headers.
   *     - response_headers: an array of response headers.
   */
  public function testingGetHeaders() {
    return array(
      'code' => $this->curl->curlErrorCode,
      'message' => $this->curl->curlErrorMessage,
      'request_headers' => $this->curl->requestHeaders,
      'response_headers' => $this->curl->responseHeaders,
      'curl' => $this->curl
    );
  }

  /**
   * Get Curl details if error happen.
   *
   * @return
   *   An array of request information. @see testingGetHeaders.
   *   FALSE if there is no error.
   */
  public function getErrors() {
    if ($this->isError()) {
      return $this->testingGetHeaders();
    }
    return FALSE;
  }

  /**
   * Get GoogleComputeEngine API Error message if error happen.
   *
   * @return
   *   String with error message.
   */
  public function getGoogleCEErrorMessage() {
    return $this->error_message;
  }

  /**
   * Determine if request has been cached.
   *
   * @return
   *   FALSE if error. Object with answer if request success.
   */
  protected function getResponse() {
    if ($this->debug) {
      backdrop_set_message('<pre>' . print_r($this->testingGetHeaders(), TRUE) . '</pre>');
    }
    if ($this->isError()) {
      if (isset($this->curl->response->error)) {
        if (isset($this->curl->response->error->message)) {
          $this->error_message = $this->curl->response->error->message;
        }
      }

      if ($message = $this->getGoogleCEErrorMessage()) {
        backdrop_set_message(t('Google Compute Engine API error message !message', array('!message' => $message)), 'error');
      }
      else {
        $code = $this->curl->errorCode;
        $message = $this->curl->errorMessage;
        backdrop_set_message(t('Communication error: !code !message', array(
          '!code' => $code,
          '!message' => $message
        )), 'error');
      }

      return FALSE;
    }
    if (is_object($this->curl->response)) {
      return (array) $this->curl->response;
    }
    if ('null' == $this->curl->response or NULL == $this->curl->response) {
      return FALSE;
    }
    return $this->curl->response;
  }

  /**
   * Perform GET request to GoogleComputeEngine API and return answer.
   *
   * @param $command
   *   String value. GoogleComputeEngine API url with tokens Like :owner, :repo and ect.
   * @param $params array
   *   Values for request and tokens for request url. Like :owner, :repo, :id
   *   and etc.
   *
   * @return array|bool|null
   *   FALSE if request failed. An array of data on success.
   */
  protected function getRequest($command, $params = array()) {

    $this->prepareCommand($command, $params);

    if ($this->getToken()) {
      $this->curl->setHeader('Authorization', 'Bearer ' . $this->getToken());
    }

    $this->curl->get(ZENCI_GOOGLE_CE_API_URI . '/' . $this->project_id . '/' . $command, $params);
    $response = $this->getResponse();
    return $response;
  }

  /**
   * Perform PUT request to GoogleComputeEngine API and return answer.
   *
   * @param $command
   *   String value. GoogleComputeEngine API url with tokens Like :owner, :repo and ect.
   * @param $params array
   *   Values for request and tokens for request url. Like :owner, :repo, :id
   *   and etc.
   *
   * @return
   *   FALSE if request failed. Object if success.
   */
  protected function putRequest($command, $params = array()) {

    $this->prepareCommand($command, $params);

    if ($this->getToken()) {
      $this->curl->setHeader('Authorization', 'Bearer ' . $this->getToken());
    }

    $this->curl->put(ZENCI_GOOGLE_CE_API_URI . '/' . $this->project_id . '/' . $command, $params);
    $response = $this->getResponse();
    return $response;
  }

  /**
   * Perform POST request to GoogleComputeEngine API and return answer.
   *
   * @param $command
   *   String value. GoogleComputeEngine API url with tokens Like :owner, :repo and ect.
   * @param $params array
   *   Values for request and tokens for request url. Like :owner, :repo, :id
   *   and etc.
   *
   * @return
   *   FALSE if request failed. Object if success.
   */
  protected function postRequest($command, $params = array()) {

    $this->prepareCommand($command, $params);
    if ($this->getToken()) {
      $this->curl->setHeader('Authorization', 'Bearer ' . $this->getToken());
    }
    $this->curl->post(ZENCI_GOOGLE_CE_API_URI  . '/' . $this->project_id . '/' . $command, $params);
    $response = $this->getResponse();
    return $response;
  }

  /**
   * Perform DELETE request to GoogleComputeEngine API and return answer.
   *
   * @param $command
   *   String value. GoogleComputeEngine API url with tokens Like :owner, :repo and ect.
   * @param $params array
   *   Values for request and tokens for request url. Like :owner, :repo, :id
   *   and etc.
   *
   * @return
   *   FALSE if request failed. Object if success.
   */
  protected function deleteRequest($command, $params = array()) {

    $this->prepareCommand($command, $params);

    if ($this->getToken()) {
      $this->curl->setHeader('Authorization', 'Bearer ' . $this->getToken());
    }

    $this->curl->delete(ZENCI_GOOGLE_CE_API_URI . '/' . $this->project_id . '/' . $command, $params, FALSE);

    $response = $this->getResponse();
    return $response;
  }

  /**
   * Replace tokens with values in request url.
   *
   * @param $command
   *   String value. GoogleComputeEngine API url with tokens Like :owner, :repo and ect.
   * @param $params array
   *   Values for request and tokens for request url. Like :owner, :repo, :id
   *   and etc.
   */
  private function prepareCommand(&$command, &$params) {
    foreach ($params as $key => $val) {
      if ($key[0] == ':') {
        $command = str_replace($key, $val, $command);
        unset($params[$key]);
      }
    }
  }

  /**
   * Get Regions information.
   * https://developers.digitalocean.com/documentation/v2/#regions
   *
   * @return
   *   Return regions information or FALSE if there is no regions or no
   *   access.
   *
   */
  public function getRegions() {
    $answer = $this->getRequest('regions');
    if(isset($answer['regions'])){
      return $answer['regions'];
    }
    return $answer;
  }

  /**
   * Get Images information.
   * https://cloud.google.com/compute/docs/reference/latest/images/list
   *
   * @return
   *   Return images information or FALSE if there is no images or no
   *   access.
   *
   */
  public function getImages() {
    $answer = $this->getRequest('global/images');
    if(isset($answer['items'])){
      return $answer['items'];
    }
    return $answer;
  }

   /**
   * Get Images information.
   * https://cloud.google.com/compute/docs/reference/latest/images/list
   *
   * @return
   *   Return images information or FALSE if there is no images or no
   *   access.
   *
   */
  public function getPublicImages($project) {

    // We update token before call to make sure that token is updated.
    $this->getToken();

    $current_project_id = $this->project_id;
    $this->project_id = $project;
    $answer = $this->getRequest('global/images');
    $this->project_id = $current_project_id;

    if(isset($answer['items'])){
      return $answer['items'];
    }
    return $answer;
  }

  /**
   * Get Image information.
   * https://developers.digitalocean.com/documentation/v2/#retrieve-an-existing-image-by-id
   *
   * @return
   *   Return images information or FALSE if there is no images or no
   *   access.
   *
   */
  public function getImage($image_id) {

    $answer = $this->getRequest('images/' . $image_id);
    if(isset($answer['image'])){
      return $answer['image'];
    }
    return $answer;
  }


  /**
   * Get aggregated machineTypes information.
   * https://cloud.google.com/compute/docs/reference/latest/machineTypes/aggregatedList
   *
   * @return
   *   Return machine types array.
   *
   */
  public function getMachineTypes($zone = '') {

    $cmd = 'aggregated/machineTypes';
    if($zone) {
      $cmd = 'zones/' . $zone . '/machineTypes';
    }

    $answer = $this->getRequest($cmd);
    if(isset($answer['items'])){
      $machineTypes = array();
      foreach($answer['items'] as $zone => $value){
        foreach($value->machineTypes as $machineType){
          $machineTypes[$machineType->name] = $machineType->name . ' ' . $machineType->description;
        }
      }
      return $machineTypes;
    }
    return $answer;
  }

  /**
   * Get zones that support provided machineType.
   * https://cloud.google.com/compute/docs/reference/latest/machineTypes/aggregatedList
   *
   * @return
   *   Return 1 level array of zones.
   *
   */
  public function getZonesByMachineType($machine_type) {

    $params = array(
      'filter' => 'name eq ' . $machine_type,
    );

    $answer = $this->getRequest('aggregated/machineTypes', $params);
    if(isset($answer['items'])){
      $zones = array();
      foreach($answer['items'] as $zone_name => $value){
        $zones[] = substr($zone_name, 6);
      }
      return $zones;
    }
    return $answer;
  }

  /**
   * Get Instance information.
   * https://cloud.google.com/compute/docs/reference/latest/instances/get
   *
   * @return
   *   Return Instance information or FALSE if there is no images or no
   *   access.
   *
   */
  public function getInstance($zone, $name) {
    $answer = $this->getRequest('zones/' . $zone . '/instances/' . $name);
    return $answer;
  }

  /**
   * Create Instance.
   * https://cloud.google.com/compute/docs/reference/latest/instances/insert
   *
   * @param $templare object
   *   An array of settings for droplet
   *     - name: The name for template. REQUIRED
   *     - machine_type: The machine type.  REQUIRED
   *     - image_name: The image name.  REQUIRED
   *     - disk_type: The disk type.  REQUIRED
   *     - disk_size: Size in GB.
   *
   * @return
   *   Return Instance information or FALSE if there is no access.
   *
   */
  public function createInstance($name, $template) {

    $zones = $this->getZonesByMachineType($template->machine_type);

    $zone = $zones[rand(0,count($zones) - 1)];
    $this->reInitCurl();

    // Need to provide key via setParam function.
    $key = new stdClass();
    $key->key = "ssh-keys";
    $key->value = $this->ssh_public_key;

    $metadata = new stdClass();
    $metadata->items[] = $key;

    $tags = new stdClass();

    $tags->items[] = 'http-server';
    $tags->items[] = 'https-server';

    $disk = new stdClass();

    $disk->type = 'PERSISTENT';
    $disk->boot = TRUE;
    $disk->mode = 'READ_WRITE';
    $disk->autoDelete = TRUE;
    $disk->deviceName = $name . "-disk";
    $disk->initializeParams = new stdClass();;
    $disk->initializeParams->sourceImage = 'projects/' . $this->project_id .'/global/images/backdrop-php53';

    $disk->initializeParams->diskType = 'projects/' . $this->project_id .'/zones/' . $zone. '/diskTypes/' . $template->disk_type;
    $disk->initializeParams->diskSizeGb = $template->disk_size;

    $network_accessConfigs = new stdClass();

    $network_accessConfigs->name = "External NAT";
    $network_accessConfigs->type = "ONE_TO_ONE_NAT";

    $network = new stdClass();
    $network->network = "projects/" . $this->project_id . "/global/networks/default";
    $network->accessConfigs = array($network_accessConfigs);

    $serviceAccount = new stdClass();

    $serviceAccount->email = $this->project_id . "@" . ZENCI_GOOGLE_CE_PROJECT_DOMAIN;
    $serviceAccount->scopes = array(ZENCI_GOOGLE_CE_INSTANCE_SCOPE);

    $scheduling = new stdClass();
    $scheduling->preemptible = TRUE;
    $scheduling->onHostMaintenance = "TERMINATE";
    $scheduling->automaticRestart = FALSE;

    $instance_settings = array(
      'name' => $name,
      'zone' => "projects/" . $this->project_id . "/zones/" . $zone,
      'machineType' => "projects/" . $this->project_id . "/zones/" . $zone ."/machineTypes/" . $template->machine_type,
      'metadata' => $metadata,
      'tags' => $tags,
      'disks' => array($disk),
      'canIpForward' => FALSE,
      'networkInterfaces' => array($network),
      'description' => 'ZenCI testing eviorment',
      'scheduling' => $scheduling,
      ':zone' => $zone,
    );

    $answer =  $this->postRequest('zones/:zone/instances', $instance_settings);

    $count = 1;
    while(TRUE){
      $count ++;
      if($count > 10){
        break;
      }
      sleep(5);
      $this->reInitCurl();
      if($data = $this->getInstance($zone, $name)) {
        if($data['status'] == 'RUNNING'){
          $ip = $data['networkInterfaces'][0]->accessConfigs[0]->natIP;
          $data['zone_name'] = $zone;
          $data['ip_address'] = $ip;
          return $data;
        }
      }
      else {
        return FALSE;
      }
    }
  }

  /**
   * Delete instance.
   * https://cloud.google.com/compute/docs/reference/latest/instances/delete
   *
   * @name instance name
   * @zone instance zone
   *
   * @return
   *   Return empty value if success or FALSE if error.
   *
   * @see SetRepoName
   * @see SetOwnerName
   */
  public function deleteInstance($zone, $name) {
    return $this->deleteRequest(
      'zones/:zone/instances/:instance', array(
        ':zone' => $zone,
        ':instance' => $name,
      )
    );
  }
}
