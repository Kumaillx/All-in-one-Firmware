#include "OTA.h"
#include "esp_ota_ops.h"

/**
 * @brief downloadUpdate downloads the updated version of firmware
 * by creating a client and updates the ESP after the whole file is
 * downloaded.
 *
 * @return true if latest update is installed
 */
bool downloadUpdate()
{
    String currentVersion = String(FIRMWARE_VERSION);

    if (latestVersion == "")
    {
        log_d("Latest version not defined in Firebase! Skipping update...");
        return true;
    }

    if (currentVersion == latestVersion)
    {
        log_d("Already running latest version [%s]", latestVersion);
        return true;
    }

    log_d("Trying to update from version %s to %s...", currentVersion, latestVersion);

    String url = CLOUD_HOST_URL + latestVersion + ".bin";

    HTTPClient http;
    http.setTimeout(15000);

    for (int retryCount = 0; retryCount < OTA_UPDATE_FAILURE_LIMIT; retryCount++)
    {
        log_d("[HTTP] Download attempt %d of %d...", retryCount + 1, OTA_UPDATE_FAILURE_LIMIT);

        if (!http.begin(url))
        {
            log_e("Unable to begin HTTP connection");
        }
        else
        {
            int httpCode = http.GET();
            log_d("[HTTP] GET... code: %d", httpCode);

            if (httpCode != HTTP_CODE_OK)
            {
                log_e("HTTP GET request failed with status code %d", httpCode);
            }
            else
            {
                int contentLength = http.getSize();
                log_d("contentLength : %d", contentLength);

                if (contentLength <= 0)
                {
                    log_e("There was no content in the response!");
                }
                else
                {   WiFiClient &stream = http.getStream();
                    if (!Update.begin(contentLength))
                    {
                        log_e("Not enough space to begin OTA!");
                    }
                    else
                    {
                        uint8_t buff[1024];
                        size_t totalWritten = 0;
                        int lastPercent = 0;

                        while (http.connected() && totalWritten < contentLength)
                        {
                            size_t len = stream.readBytes(buff, sizeof(buff));
                            if (len > 0)
                            {
                                totalWritten += Update.write(buff, len);
                                int percent = (totalWritten * 100) / contentLength;
                                if (percent - lastPercent >= 2)
                                {
                                    lastPercent = percent;
                                    log_d("OTA Progress: %d%%", percent);
                                }
                            }
                        }
                        if (Update.end(true))
                        {
                            if (Update.isFinished())
                            {
                                Serial.println("OTA update complete. Rebooting...");
                                esp_ota_mark_app_valid_cancel_rollback();
                                ESP.restart();
                            }
                            else
                            {
                                Serial.println("OTA not finished properly.");
                            }
                        }
                        else
                        {
                            Serial.printf("OTA failed. Error #: %d\n", Update.getError());
                        }
                    }
                }
            }

            http.end();
        }

        int retryDelay = pow(2, retryCount) * 1000;
        log_d("Retrying update in %d seconds...", retryDelay / 1000);
        delay(retryDelay);
    }

    return false;
}