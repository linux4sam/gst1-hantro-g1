/* atmel_drm.h
 *
 */

#ifndef _UAPI_ATMEL_DRM_H_
#define _UAPI_ATMEL_DRM_H_

#include <drm/drm.h>

#define DRM_ATMEL_GEM_GET		0x00

#define DRM_IOCTL_ATMEL_GEM_GET		DRM_IOWR(DRM_COMMAND_BASE + \
					DRM_ATMEL_GEM_GET, struct drm_mode_map_dumb)

#endif /* _UAPI_ATMEL_DRM_H_ */
