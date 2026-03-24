#ifndef __OTA_H__
#define __OTA_H__

#include <HTTPClient.h>
#include <Update.h>
#include "config.h"

#define AWS_S3_BUCKET "abb-meter-firmware-updates"
#define AWS_S3_REGION "eu-north-1"
#define OTA_UPDATE_FAILURE_LIMIT 3

#if NUM_OF_SLOTS == 1
#define CLOUD_HOST_URL "https://" AWS_S3_BUCKET ".s3." AWS_S3_REGION ".amazonaws.com/single_slot/firmware_"
#elif NUM_OF_SLOTS == 2
#define CLOUD_HOST_URL "https://" AWS_S3_BUCKET ".s3." AWS_S3_REGION ".amazonaws.com/double_slot/firmware_"
#elif NUM_OF_SLOTS == 3
#define CLOUD_HOST_URL "https://" AWS_S3_BUCKET ".s3." AWS_S3_REGION ".amazonaws.com/three_slot/firmware_"

#endif

extern String latestVersion;

bool downloadUpdate();

#endif