/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 */

#ifndef SwapGroup_h
#define SwapGroup_h

#include <wtf/PassOwnPtr.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/HashSet.h>

namespace Olympia {
    namespace WebKit {
        class WebPage;

        class Swappable {
        public:
            virtual ~Swappable() {}

            virtual void swapBuffers() = 0;
            virtual void didCommitSwapGroup() {}
        };

        class SwapGroup {
        public:
            SwapGroup(WebPage* page);
            ~SwapGroup();

            static SwapGroup* current();

            static bool isCommitting();
            void commit();

            void add(Swappable* swappable)
            {
                m_swappables.add(swappable);
            }

        private:
            mutable Mutex m_commitMutex;
            bool m_committing;
            WebPage* m_webPage;
            HashSet<Swappable*> m_swappables;
        };
    }
}

#endif // SwapGroup_h
