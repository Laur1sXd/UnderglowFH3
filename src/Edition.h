#pragma once

#ifndef UG_SUPPORT_EXTRA_OPTIONS
#define UG_SUPPORT_EXTRA_OPTIONS 0
#endif
#ifndef UG_SUPPORT_HORIZON3_EXTENDED
#define UG_SUPPORT_HORIZON3_EXTENDED 1
#endif
#ifndef UG_SUPPORT_HORIZON_RADIO_NETWORK
#define UG_SUPPORT_HORIZON_RADIO_NETWORK 0
#endif

constexpr bool supportExtraOptions = UG_SUPPORT_EXTRA_OPTIONS != 0;
constexpr bool supportHorizon3Extended = UG_SUPPORT_HORIZON3_EXTENDED != 0;
