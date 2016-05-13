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
   * Constructor.
   */
  public function __construct() {
    $this->reInitCurl();
    $this->setDebug();
    $this->private_key = zenci_google_ce_get_private_key();
    $this->project_id = zenci_google_ce_get_project_id();
    $config = config('zenci_google_ce.settings');
    $token = $config->get('token');
    if($token) {
      $this->token = $token;
      $this->token_expire = $config->get('token_expire');    
    }else{
      $this->getToken();
    }
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
   * Set token for GoogleComputeEngine API.
   *
   * @param $token
   *   Token to access GoogleComputeEngine API.
   * @param $expire_timestamp
   *   Expiration Timestamp.
   */
  private function setToken($token, $expire) {
    $this->token = $token;
    $this->token_expire = $expire;    
    $config = config('zenci_google_ce.settings');
    $config->set('token', $token);
    $config->set('token_expire', $expire);
    $config->save();
  }

  /**
   * Get store token.
   *
   * @return
   *   A string value. Stored token for GoogleComputeEngine API access.
   */
  public function getToken() {
    
    if(empty($this->token) or $this->token_expire < time() + 300 ) {
      
      // Check file first. Maybe it is updated already.
      $config = config('zenci_google_ce.settings');
      $token = $config->get('token');
      $token_expire = $config->get('token_expire');
      
      if($token && $token_expire > time() + 300 ) {
        $this->token = $token;
        $this->token_expire = $token_expire;
      }
      else{
        // Time to get new token.
        $this->updateToken();
      }
    }
    return $this->token;
  }
  
  /**
   * Update Token.
   */
  private function updateToken() {
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
   * Update Token.
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
      $this->curl->setHeader('Token', $this->getToken());
    }

    $this->curl->put(ZENCI_GOOGLE_CE_API_URI . '/' . $command, $params);
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

    $this->curl->delete(ZENCI_GOOGLE_CE_API_URI . '/' . $command, $params);
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
   * Create droplet.
   * https://developers.digitalocean.com/documentation/v2/#create-a-new-droplet
   *
   * @param $settings array
   *   An array of settings for droplet
   *     - name: The name for droplet. REQUIRED
   *     - region: The region for droplet.  REQUIRED
   *     - size: The size for droplet.  REQUIRED
   *     - image: The image for droplet.  REQUIRED
   *     - ssh_keys: The ssh_keys for droplet.
   *     - backups: The backups for droplet.
   *     - ipv6: The ipv6 for droplet. TRUE or FALSE
   *     - user_data: The user_data for droplet.
   *     - private_networking: The private_networking for droplet.
   *
   * @return
   *   Return droplet information or FALSE if there is no access.
   *
   */
   
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
  public function createInstance($template) {
    
    $zones = $this->getZonesByMachineType($template->machine_type);
    
backdrop_set_message('<pre>' . print_r($zones,TRUE). '</pre>');
    
    $zone = $zones[rand(0,count($zones) - 1)];
    $this->reInitCurl();
    
    $key = new stdClass();
    $key->key = "ssh-keys";
    $key->value = "root:ssh-rsa AAAAB3NzaC1yc2EAAAABIwAAAQEAtU034MgQ7NRqM1neyR/VzJuEfH75klZjXSjGUHcRbVnxBP6fsf/tPRT6cmYMqkZtWZfl3HNV5+x2Wb0me4lww2ZFWnN/chyYNh6uRmzxf+fgrlCvjY1a71TVv2AjILM/6gK68oc0575WEhEs0nEDi8Mj3kjPIwmBQa2H+Ul9gqjCnJchVtfNKDf8vK4Nf2oTemzq5U1AIJu+6ha/EY6gCin1U2OOKZHE8YoG7QS980YQWqlk+9oZueXMQuOc1SZDr8MJqbu+5jZpW26dsYaeC+fO7LYT+vitOWIe0XjpTQkaGesfNzkqfg4nPZeSsX4wtyVabdbhSK0JZPEErd6PCQ== root@coral.vps-private.net";
    
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
    $disk->deviceName = 'test-' . $template->name. "-zen-ci";
    $disk->initializeParams = new stdClass();;
    $disk->initializeParams->sourceImage = 'projects/' . $this->project_id .'/global/images/php53';
    
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
    
    $instance_settings = new stdClass();
    $instance_settings->name = 'test-' . $template->name. "-zen-ci";
    $instance_settings->zone = "projects/" . $this->project_id . "/zones/" . $zone;
    $instance_settings->machineType = "projects/" . $this->project_id . "/zones/" . $zone ."/machineTypes/" . $template->machine_type;
    $instance_settings->metadata = $metadata;
    $instance_settings->tags = $tags;
    $instance_settings->disks = array($disk);
    $instance_settings->canIpForward = FALSE;
    $instance_settings->networkInterfaces = array($network);
    $instance_settings->description = 'ZenCI testing eviorment';
    $instance_settings->scheduling = new stdClass();;
    $instance_settings->scheduling->preemptible = TRUE;
    $instance_settings->scheduling->onHostMaintenance = "TERMINATE";
    $instance_settings->scheduling->automaticRestart = FALSE;
    
    $instance_settings->serviceAccounts = array($serviceAccount);

//    backdrop_set_message('<pre>' . json_encode($instance_settings). '</pre>');
      
    $answer =  $this->postRequest('zones/' . $zone . '/instances', (array)$instance_settings);
    backdrop_set_message('<pre>' . print_r($answer, true). '</pre>');

    sleep(5);
    $this->reInitCurl();
    $instance_new = $this->getInstance($zone, 'test-' . $template->name. "-zen-ci");
    backdrop_set_message('<pre>' . print_r($instance_new, true). '</pre>');


    sleep(5);
    $this->reInitCurl();
    $instance_new = $this->getInstance($zone, 'test-' . $template->name. "-zen-ci");
    backdrop_set_message('<pre>' . print_r($instance_new, true). '</pre>');

    sleep(5);
    $this->reInitCurl();
    $instance_new = $this->getInstance($zone, 'test-' . $template->name. "-zen-ci");

    backdrop_set_message('<pre>' . print_r($instance_new, true). '</pre>');

    return $answer;

  }

  /**
   * Update hook for repository.
   *
   * @param $settings array
   *   An array of settings for hook
   *     - url: The URL to which the payloads will be delivered.
   *     - secret: If provided, payloads will be delivered with an Signature
   *   header.
   *
   * @return
   *   Return hook information or FALSE if there is no access.
   *
   * @see SetRepoName
   * @see SetOwnerName
   */
  public function updateHook($settings) {
    $settings[':owner'] = $this->owner_name;
    $settings[':repo'] = $this->repo_name;
    return $this->putRequest('repos/:owner/:repo/hook', $settings);
  }

  /**
   * Delete hook for repository.
   * @return
   *   Return empty value if success or FALSE if error.
   *
   * @see SetRepoName
   * @see SetOwnerName
   */
  public function deleteHook() {
    return $this->deleteRequest(
      'repos/:owner/:repo/hook', array(
        ':owner' => $this->owner_name,
        ':repo' => $this->repo_name,
      )
    );
  }
}
