// ThumbnailPool.h
// eiffelbs-ui - bounded pool of audio thumbnails that share one cache
// (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// A host that draws MANY clips (a timeline) needs a thumbnail per visible clip
// and a way to keep the set bounded without a second copy of JUCE's cache
// lifecycle rules. This is the whole component: an owner for one cache, a
// uid-keyed set of thumbnails, and the draw helper the hosts were all
// writing by hand.
//
// THE DESTRUCTION ORDER IS THE POINT. A juce::AudioThumbnail holds a reference
// to its cache, so the cache must OUTLIVE every thumbnail. Getting that wrong
// is not a leak you see - it is a jassert from JUCE's leak detector at process
// exit, or worse. Declaring the cache FIRST and the thumbnails AFTER makes
// the order correct by construction: members are destroyed in reverse
// declaration order, so the thumbnails die first and the cache last. A host
// that hand-rolled this had to remember the ordering and comment it; here the
// ordering is the code.
//
// The pool evicts the least-recently-USED thumbnail when it exceeds maxRows,
// so a host that keeps every clip it ever scrolled past cannot grow without
// bound. A host that knows which clips are visible may also call remove() to
// drop one on purpose.

// NOT IN THE UMBRELLA eiffelbs.h, ON PURPOSE. This header pulls
// <juce_audio_utils> for juce::AudioThumbnailCache, so it compiles the whole
// juce_audio_devices module inside every TU that includes the umbrella - the
// same cost Waveform.h carries. Include this header directly and link
// juce::juce_audio_utils. See the note at the top of eiffelbs.h.

#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include "eiffelbs/Theme.h"

namespace ebs
{

    class ThumbnailPool
    {
    public:
        explicit ThumbnailPool (int maxRows = 16)
            : cache (juce::jmax (maxRows, kCacheSlots)),
              maxRows (juce::jmax (1, maxRows))
        {
        }

        ~ThumbnailPool() { clear(); }

        int size() const noexcept { return (int) thumbs.size(); }

        // === Sources ========================================================

        /** Create or replace the thumbnail for `uid`. An empty file stores
            nothing and reports totalSeconds() == 0, which is the host's
            signal to draw nothing rather than wait forever. */
        void setFile (const juce::String& uid, const juce::File& file)
        {
            remove (uid);
            if (uid.isEmpty() || ! file.existsAsFile())
                return;
            auto t = std::make_unique<juce::AudioThumbnail> (
                kSamples, formats(), cache);
            t->setSource (new juce::FileInputSource (file));
            thumbs[uid] = std::move (t);
            touch (uid);
        }

        void remove (const juce::String& uid) { thumbs.erase (uid); }
        void clear() { thumbs.clear(); }

        /** The thumbnail for `uid`, or nullptr. Non-const: drawing goes
            through AudioThumbnail::drawChannels, which is not a const call,
            and the pool owns these objects outright. */
        juce::AudioThumbnail* get (const juce::String& uid)
        {
            auto it = thumbs.find (uid);
            return it != thumbs.end() ? it->second.get() : nullptr;
        }

        /** The uids currently held, for the host that computes which
            clips are visible and drops the rest - the timeline's
            "keep one per visible clip" loop. */
        std::vector<juce::String> keys() const
        {
            std::vector<juce::String> out;
            out.reserve (thumbs.size());
            for (const auto& entry : thumbs)
                out.push_back (entry.first);
            return out;
        }

        /** Const read for the hosts that only measure. */
        const juce::AudioThumbnail* find (const juce::String& uid) const
        {
            auto it = thumbs.find (uid);
            return it != thumbs.end() ? it->second.get() : nullptr;
        }

        double totalSeconds (const juce::String& uid) const
        {
            const auto* t = find (uid);
            return t != nullptr ? t->getTotalLength() : 0.0;
        }

        // === Draw ===========================================================

        /** Draw `uid`'s waveform into `rect`, clipped, in `colour`.

            This is the shape every timeline clip drew by hand: save, clip to
            the rect, set the colour, drawChannels, restore. The colour and
            the gain stay the host's - a clip waveform is a texture in the
            lane's language, not a button. `endTime` < 0 draws to the end of
            the thumbnail; a host that keeps a clip SHORTER than its source
            passes the clip's own end. `startTime` is the source offset the
            rect starts at: a clip trimmed on the LEFT shows
            [trim, trim+duration], so the waveform shifts instead of lying
            about what the left edge plays. */
        void draw (juce::Graphics& g, const juce::String& uid,
                   const juce::Rectangle<float>& rect,
                   juce::Colour colour, float gain = 1.0f,
                   double startTime = 0.0, double endTime = -1.0)
        {
            auto* t = get (uid);
            if (t == nullptr || rect.isEmpty())
                return;
            const double total = t->getTotalLength();
            const double start = juce::jlimit (0.0, total, startTime);
            const double end = endTime >= 0.0
                ? juce::jmin (endTime, total) : total;
            if (end <= start)
                return;   // trimmed to nothing: a valid silent clip
            const auto ir = rect.toNearestInt();
            g.saveState();
            g.reduceClipRegion (ir);
            g.setColour (colour);
            t->drawChannels (g, ir, start, end, gain);
            g.restoreState();
        }

        // === Test hooks ======================================================

        juce::Rectangle<int> clipForTest (const juce::String& uid) const
        {
            juce::Rectangle<int> r;
            const auto* t = find (uid);
            if (t != nullptr)
                r.setSize ((int) t->getTotalLength(), 1);
            return r;
        }

    private:
        static constexpr int kSamples = 512;
        /** The cache holds a few more slots than the pool keeps: the pool
            evicts by LRU, so thumbnails are created and destroyed, and a
            cache as small as the pool would recycle a slot on every eviction. */
        static constexpr int kCacheSlots = 64;

        // DECLARED FIRST: the cache must outlive every thumbnail (they hold a
        // reference to it), and reverse declaration order destroys the
        // thumbnails first. A comment here would be the only record of that
        // rule - this is the code.
        juce::AudioThumbnailCache cache;

        int maxRows;
        // uid -> thumbnail; the map is the ORDER: insertion order is the LRU
        // order, which is what evictLRU walks.
        std::map<juce::String, std::unique_ptr<juce::AudioThumbnail>> thumbs;
        std::vector<juce::String> lru;

        static juce::AudioFormatManager& formats()
        {
            static juce::AudioFormatManager mgr;
            static const bool once = []
                { mgr.registerBasicFormats(); return true; } ();
            juce::ignoreUnused (once);
            return mgr;
        }

        void touch (const juce::String& uid)
        {
            // Move uid to the back of the LRU list.
            lru.erase (std::remove (lru.begin(), lru.end(), uid), lru.end());
            lru.push_back (uid);
            while ((int) lru.size() > maxRows)
            {
                const auto victim = lru.front();
                lru.erase (lru.begin());
                thumbs.erase (victim);
            }
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThumbnailPool)
    };

} // namespace ebs
