# Ocean rendering - ACM / SIGGRAPH citation notes

This file records **accurate venues and titles** for commonly confused ocean-related publications. It is not an implementation status document; see `docs/RENDERERS.md` (water/flowmap) and renderer code for engine features.

## Corrections (vs. informal screenshots or paraphrases)

1. **“The Sea Is Your Mirror”**  
   - **Not** SIGGRAPH 2011. It appears in the **8th ACM SIGGRAPH Conference on Motion in Games (MIG), 2015** - verify the exact entry in the [ACM Digital Library](https://dl.acm.org/).

2. **“Real-Time Ocean Rendering”**  
   - Often a **section or category label** in proceedings or course notes, **not** a single guaranteed paper title.  
   - A clearly identifiable related work is **“Real-time Animation and Rendering of Ocean Whitecaps”** - use the ACM DL record for the exact year, venue, and authors.

3. **“Abstract Ocean Waves” (Poster)**  
   - Cited as a **poster** in the ACM Digital Library; treat the DL bibliographic record as canonical for year and venue.

4. **“Ocean Mission on Cars 2”**  
   - Production / **talk**-style material on the ocean work for *Cars 2* (sometimes summarized informally as a “stormy ocean” talk). It is **not** interchangeable with the MIG 2015 paper above.

## Practical use in this repo

- Prefer **exact titles + venue + year** from ACM DL before adding references to roadmaps or comments.  
- When implementing Gerstner waves, FFT oceans, or whitecaps, cite the **specific** paper whose math you follow (e.g. whitecaps paper vs. a MIG camera/reflection technique).

See also **`docs/REALTIME_RENDERING_READING_LIST.md`** for a prioritized reading list with verified JCGT links.
