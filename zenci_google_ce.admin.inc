<?php
/**
 * @file
 * Administration pages for DigitalOcean integration.
 */

 /**
 * Menu callback for admin/config/system/qa.
 */
function zenci_google_ce_settings() {
  $config = config('zenci_google_ce.settings');

  // Use setting.php values if exists.
  $project_id = settings_get('zenci_google_ce_project_id');

  $form['project_id'] = array(
    '#type' => 'textfield',
    '#title' => t('Project id'),
    '#default_value' => !empty($project_id) ? $project_id : $config->get('project_id'),
    '#description' => t('You can obtain Project id here https://console.cloud.google.com/iam-admin/projects'),
    '#disabled' => !empty($project_id) ? TRUE : FALSE,
  );

  // Use setting.php values if exists.
  $private_key = settings_get('zenci_google_ce_private_key');

  $form['private_key'] = array(
    '#type' => 'textarea',
    '#title' => t('Private Key'),
    '#default_value' => !empty($private_key) ? $private_key : $config->get('private_key'),
    '#description' => t('You need to get Service Account Key here: https://console.cloud.google.com/iam-admin/serviceaccounts/serviceaccounts-zero'),
    '#disabled' => !empty($private_key) ? TRUE : FALSE,
  );

  $token = $config->get('token');

  if($token){
    $token_expire = $config->get('token_expire');
    $now = time();

    $description = '';
    if($token_expire < $now){
      $description = t('Token expired.');
    }
    else{
      $description = t('This token expire in !sec seconds', array('!sec' => $token_expire - $now));
    }

    $form['token'] = array(
      '#type' => 'textfield',
      '#title' => t('Current token'),
      '#default_value' => $config->get('token'),
      '#description' => $description,
      '#disabled' => TRUE,
    );
  }


  $form['actions']['#type'] = 'actions';
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save configuration'),
  );

  return $form;
}

/**
 * Submit handler for the qa_settings() form.
 */
function zenci_google_ce_settings_submit($form, &$form_state) {

  $config = config('zenci_google_ce.settings');

  // Use setting.php values if exists.
  $project_id = settings_get('zenci_google_ce_project_id');

  if (empty($project_id)) {
    $config->set('project_id', $form_state['values']['project_id']);
  }


  // Use setting.php values if exists.
  $private_key = settings_get('zenci_google_ce_private_key');

  if (empty($private_key)) {
    $config->set('private_key', $form_state['values']['private_key']);
  }


  $config->save();

  backdrop_set_message(t('The configuration options have been saved.'));
}

/**
 * Menu callback; Display a list of all templates.
 */
function zenci_google_ce_templates() {
  $templates = db_select('zenci_google_ce_templates', 'wr')
    ->fields('wr')
    ->orderBy('name', 'DESC')
    ->execute()
    ->fetchAll();

  $rows = array();
  foreach ($templates as $template) {
    $row = array();
    $row[] = array('data' => $template->name);
    $row[] = array('data' => $template->machine_type);
    $row[] = array('data' => $template->image_name);
    $row[] = array('data' => $template->disk_type);
    $row[] = array('data' => $template->disk_size);
    $links['delete'] = array(
      'title' => t('Delete'),
      'href' => "admin/config/system/google_ce/" . $template->id . "/delete",
    );
    $row[] = array(
      'data' => array(
        '#type' => 'operations',
        '#links' => $links,
      ),
    );
    $rows[] = $row;
  }
  $output['link'] = array(
    '#theme' => 'links__template_add',
    '#links' => array(
      'add' => array(
        'title' => t('Add template'),
        'href' => "admin/config/system/google_ce/template_add",
      ),
    ),
    '#attributes' => array('class' => array('button-primary')),
  );
  $header = array(t('Name'), t('Machine type'), t('Image'), t('Disk type'), t('Disk size'),   t('Operations'));

  $output['table'] = array(
    '#theme' => 'table__zenci_google_ce_templates',
    '#rows' => $rows,
    '#header' => $header,
    '#empty' => t('No templates yet.'),
  );
  return $output;
}

/**
 * Add template form.
 */
function zenci_google_ce_template($form, &$form_state) {

  $GoogleComputeEngineAPI = zenci_google_ce_get_class();
  $form['name'] = array(
    '#type' => 'textfield',
    '#title' => t('Name'),
    '#description' => t('This is a test task name.'),
    '#required' => TRUE,
  );

  $MachineTypes = $GoogleComputeEngineAPI->getMachineTypes();

  $form['machine_type'] = array(
    '#type' => 'select',
    '#title' => t('Machine type'),
    '#options' => $MachineTypes,
    '#description' => t('Please machine type.'),
  );


  $public_projects = array(
    'centos-cloud',
    'google-containers',
    'coreos-cloud',
    'debian-cloud',
    'rhel-cloud',
    'opensuse-cloud',
    'suse-cloud',
    'ubuntu-os-cloud',
    'windows-cloud',
  );

  $private_images['private'] = $GoogleComputeEngineAPI->getImages();

  foreach($public_projects as $public_project) {
    $private_images[$public_project] = $GoogleComputeEngineAPI->getPublicImages($public_project);
  }


  $image_options = array();
  foreach($private_images as $project => $images) {
    foreach($images as $image){
      if(isset($image->deprecated)){
        continue;
      }
      $image_options[$project . ":" . $image->name] = $image->name . ' : ' . $image->description;
    }
  }

  $form['image'] = array(
    '#type' => 'select',
    '#title' => t('Image'),
    '#options' => $image_options,
    '#description' => t('Please select image.'),
  );

  $disk_options = array(
    'pd-ssd' => t('SSD persistent disk'),
    'pd-standard' => t('Standard persistent disk'),
  );

  $form['disk_type'] = array(
    '#type' => 'select',
    '#title' => t('Disk type'),
    '#options' => $disk_options,
    '#description' => t('Please select disk type.'),
  );

  $form['disk_size'] = array(
    '#type' => 'textfield',
    '#title' => t('Disk size in GB'),
    '#description' => t('Numbers only. No less than 10 GB'),
    '#required' => TRUE,
  );

  $form['actions']['#type'] = 'actions';
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Add template'),
  );

  return $form;
}

/**
 * Validate handler for the zenci_google_ce_template() form.
 */
function zenci_google_ce_template_validate($form, &$form_state) {
  $template = db_select('zenci_google_ce_templates', 'gh')
    ->fields('gh')
    ->condition('name', $form_state['values']['name'])
    ->execute()
    ->fetchObject();

  if ($template) {
    form_set_error('name', t('Name value should be unique.'));
  }
}

/**
 * Submit handler for the zenci_google_ce_template() form.
 */
function zenci_google_ce_template_submit($form, &$form_state) {

  $template = array(
    'name' => $form_state['values']['name'],
    'machine_type' => $form_state['values']['machine_type'],
    'image_name' => $form_state['values']['image'],
    'disk_type' => $form_state['values']['disk_type'],
    'disk_size' => $form_state['values']['disk_size'],
  );
  if(FALSE !== backdrop_write_record('zenci_google_ce_templates', $template)){
    backdrop_set_message(t('Template !name created', array(
      '!name' => $template['name'],
    )));
  }
  backdrop_goto('admin/config/system/google_ce/list');
}

/**
 * Remove template form.
 */
function zenci_google_ce_template_remove($form, &$form_state, $template_id) {
  $template = db_select('zenci_google_ce_templates', 'gh')
    ->fields('gh')
    ->condition('id', $template_id)
    ->execute()
    ->fetchObject();

  if (empty($template)) {
    backdrop_goto('admin/config/system/google_ce/list');
  }

  $form_state['template'] = $template;

  $form['message'] = array(
    '#markup' => t('Are you sure want to delete this template?'),
  );
  $form['actions']['#type'] = 'actions';
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Delete'),
  );

  return $form;

}

/**
 * Submit handler for the zenci_google_ce_template_remove() form.
 */
function zenci_google_ce_template_remove_submit($form, &$form_state) {
  $template = $form_state['template'];
  db_delete('zenci_google_ce_templates')
    ->condition('id', $template->id)
    ->execute();
  backdrop_set_message(t('Template deleted!'));
  backdrop_goto('admin/config/system/google_ce/list');
}

/**
 * Add instance form.
 */
function zenci_google_ce_instance($form, &$form_state) {

  $templates = db_select('zenci_google_ce_templates', 'wr')
    ->fields('wr')
    ->orderBy('name', 'DESC')
    ->execute()
    ->fetchAll();

  $options = array();
  foreach ($templates as $template) {
    $options[$template->name] = $template->name . " " . $template->machine_type . " " . $template->image_name . " " . $template->disk_size . " " . $template->disk_type;
  }
  $form['name'] = array(
    '#type' => 'select',
    '#title' => t('Template name'),
    '#options' => $options,
    '#description' => t('Please select template name.'),
  );


  $form['actions']['#type'] = 'actions';
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Add instance'),
  );

  return $form;
}

/**
 * Submit handler for the zenci_google_ce_instance() form.
 */
function zenci_google_ce_instance_submit($form, &$form_state) {

  $name = 'test-' . time();
  $template_name = $form_state['values']['name'];
  $instance = zenci_google_ce_create_instance($name, $template_name);

  if($instance){
    backdrop_set_message(t('Instance created'));
    backdrop_set_message('<pre>' . print_r($instance,true). '</pre>');
  }
}

/**
 * Del instance form.
 */
function zenci_google_ce_instance_del($form, &$form_state) {

  $instances = db_select('zenci_google_ce_instances', 'wr')
    ->fields('wr')
    ->orderBy('name', 'DESC')
    ->execute()
    ->fetchAll();

  $options = array();
  foreach ($instances as $instance) {
    $options[$instance->id] = $instance->name . " ip: " . $instance->ip . "  zone: " .$instance->zone;
  }
  $form['name'] = array(
    '#type' => 'select',
    '#title' => t('Instances'),
    '#options' => $options,
    '#description' => t('Please select instance.'),
  );


  $form['actions']['#type'] = 'actions';
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Del instance'),
  );

  return $form;
}

/**
 * Submit handler for the zenci_google_ce_instance_del() form.
 */
function zenci_google_ce_instance_del_submit($form, &$form_state) {
  $instance_id = $form_state['values']['name'];

  $instance = db_select('zenci_google_ce_instances', 'gh')
    ->fields('gh')
    ->condition('id', $instance_id)
    ->execute()
    ->fetchObject();

  if(zenci_google_ce_delete_instance($instance)){
    backdrop_set_message(t('Instance !name deleted!', array('!name' => $instance->name )));
  }
}

