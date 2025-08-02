#include <curl/curl.h>
#include <curl/easy.h>

void get_http(char *url) {
  CURL *curl = curl_easy_init();
  if (curl) {
    CURLcode res;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
  }
}
