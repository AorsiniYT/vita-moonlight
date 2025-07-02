## alpha2
### Host Discovery & UI (June 2025)
- Unified and improved host discovery system for both PSVita and Windows.
- mDNS discovery logic is now centralized, thread-safe, and cross-platform.
- The discovered hosts list on Windows is now properly cleared and refreshed when entering/leaving the "Add Host" tab, matching Vita behavior.
- Fixed UI refresh and duplicate issues for host cards on Windows.
- Refactored the global getDiscoveredHostsWin() function for safe access and to avoid build errors.
- Improved memory management and tab destruction to prevent leaks and unexpected behavior.
- Minor improvements to host card (PCCard) visuals and dialogs.

### UI and Host Discovery Improvements (June 2025)
- Modernized the host discovery UI using Borealis.
- Each detected PC is shown as a visual card (PCCard) with icon and name, selectable with focus/click feedback.
- Clicking a card shows a dialog with PC details and i18n-ready buttons.
- Loading spinner (ProgressSpinner) and "Search PC automatically" label are now centered and visible during search.
- Removed all references to loader/spinnerBox in C++ and XML.
- Spinner and label are managed via visibility from C++ using a spinner_row container.
- mDNS discovery thread is portable and does not block exit or navigation.
- All UI and dialog strings are i18n-ready.
- Fixed lifetime bugs, visual feedback, and improved cross-platform robustness.
- Improved error handling and added detailed logs for debugging.

## alpha1
### Initial Release (commit ef94343c)
- First public release of Moonlight Vita for PSVita and PC.
- Basic host discovery and connection functionality.
- Initial UI and configuration screens.
- Foundation for multiplatform support (PSVita/Windows).

