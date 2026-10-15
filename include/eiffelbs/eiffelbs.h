// eiffelbs.h
// eiffelbs-ui - umbrella header for the whole library.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// THE TWO AUDIO WIDGETS ARE NOT IN THIS UMBRELLA, ON PURPOSE. Waveform.h and
// ThumbnailPool.h need juce::AudioThumbnail, so they include
// <juce_audio_utils/juce_audio_utils.h>. In a header-only library that include
// is not a link-time cost: it is a COMPILE cost paid by EVERY translation unit
// that includes this umbrella, because it drags the whole juce_audio_devices
// module into the consumer's build. Plugin projects pay twice for it. Their TUs
// include <windows.h> without NOMINMAX (the ARA SDK and the VST3 plugin-client
// chain pull it bare), and juce_UMPEndpoint.h calls std::min WITHOUT
// parentheses, so the Windows min/max function-like macros expand that into
// std::(...) and MSVC fails (C2589 + C2059). A host that wants these widgets
// includes them directly and links the module:
//
//   #include <eiffelbs/Waveform.h>        // + juce::juce_audio_utils
//   #include <eiffelbs/ThumbnailPool.h>

#pragma once

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"
#include "eiffelbs/LookAndFeel.h"
#include "eiffelbs/IconButton.h"
#include "eiffelbs/IconTextButton.h"
#include "eiffelbs/MenuChevronButton.h"
#include "eiffelbs/DisclosureChevron.h"
#include "eiffelbs/ExpansibleFrame.h"
#include "eiffelbs/FileGripButton.h"
#include "eiffelbs/FramedBody.h"
#include "eiffelbs/Knob.h"
#include "eiffelbs/MorphSlider.h"
#include "eiffelbs/PowerToggle.h"
#include "eiffelbs/ToggleSwitch.h"
#include "eiffelbs/StatusBar.h"
#include "eiffelbs/BusyIndicator.h"
#include "eiffelbs/TextSanitize.h"
#include "eiffelbs/HelpBubble.h"
#include "eiffelbs/PathRow.h"
#include "eiffelbs/DataList.h"
#include "eiffelbs/Sidebar.h"
#include "eiffelbs/BandGrip.h"
#include "eiffelbs/Avatar.h"
#include "eiffelbs/ChatBubble.h"
#include "eiffelbs/PillTabSwitch.h"
#include "eiffelbs/TextReadability.h"
