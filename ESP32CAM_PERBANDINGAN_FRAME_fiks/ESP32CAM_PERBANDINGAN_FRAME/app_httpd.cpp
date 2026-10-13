#include "esp_http_server.h"
#include "esp_camera.h"

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=frame";
static const char* _STREAM_BOUNDARY = "\r\n--frame\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  char part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if(res != ESP_OK) return res;

  while(true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      return ESP_FAIL;
    }

    httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    size_t hlen = snprintf(part_buf, 64, _STREAM_PART, fb->len);
    httpd_resp_send_chunk(req, part_buf, hlen);
    httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);

    esp_camera_fb_return(fb);
  }
  return res;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  httpd_handle_t stream_httpd = NULL;

  httpd_uri_t stream_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  httpd_start(&stream_httpd, &config);
  httpd_register_uri_handler(stream_httpd, &stream_uri);
}
