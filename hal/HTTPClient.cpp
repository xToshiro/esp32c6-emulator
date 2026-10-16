#include "HTTPClient.h"

std::vector<HttpCapturedRequest> HTTPClient::_capturedRequests;
int HTTPClient::_simulatedResponseCode = 200;
std::function<void(const HttpCapturedRequest&)> HTTPClient::_onCapturedCallback = nullptr;
