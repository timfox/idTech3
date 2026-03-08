# DownloadInterFonts.cmake
# Downloads Inter font (v4.1) when BUILD_FREETYPE is enabled and fonts are missing.
# Inter is used as the default UI font when r_font is set to fonts/Inter-Regular.
# License: SIL Open Font License 1.1 (https://github.com/rsms/inter)

set(INTER_VERSION "4.1")
set(INTER_URL "https://github.com/rsms/inter/releases/download/v${INTER_VERSION}/Inter-${INTER_VERSION}.zip")
set(INTER_FONTS_DIR "${CMAKE_SOURCE_DIR}/base/fonts")
set(INTER_REGULAR "${INTER_FONTS_DIR}/Inter-Regular.ttf")
set(INTER_BOLD "${INTER_FONTS_DIR}/Inter-Bold.ttf")

if(BUILD_FREETYPE AND NOT ANDROID)
	if(NOT EXISTS "${INTER_REGULAR}" OR NOT EXISTS "${INTER_BOLD}")
		message(STATUS "Inter font: downloading v${INTER_VERSION} (missing from base/fonts/)")
		set(INTER_ZIP "${CMAKE_BINARY_DIR}/Inter-${INTER_VERSION}.zip")
		set(INTER_EXTRACT_DIR "${CMAKE_BINARY_DIR}/Inter-extract")

		file(DOWNLOAD "${INTER_URL}" "${INTER_ZIP}"
			STATUS DOWNLOAD_STATUS
			SHOW_PROGRESS
			TIMEOUT 60)

		list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
		if(STATUS_CODE EQUAL 0)
			file(MAKE_DIRECTORY "${INTER_EXTRACT_DIR}")
			file(ARCHIVE_EXTRACT INPUT "${INTER_ZIP}" DESTINATION "${INTER_EXTRACT_DIR}")

			# Inter zip structure: Inter-4.1/Inter/static/ttf/*.ttf or similar
			file(GLOB INTER_SRC_REGULAR
				"${INTER_EXTRACT_DIR}/Inter-Regular.ttf"
				"${INTER_EXTRACT_DIR}/*/Inter-Regular.ttf"
				"${INTER_EXTRACT_DIR}/*/*/Inter-Regular.ttf"
				"${INTER_EXTRACT_DIR}/*/*/*/Inter-Regular.ttf"
				"${INTER_EXTRACT_DIR}/*/*/*/*/Inter-Regular.ttf")
			file(GLOB INTER_SRC_BOLD
				"${INTER_EXTRACT_DIR}/Inter-Bold.ttf"
				"${INTER_EXTRACT_DIR}/*/Inter-Bold.ttf"
				"${INTER_EXTRACT_DIR}/*/*/Inter-Bold.ttf"
				"${INTER_EXTRACT_DIR}/*/*/*/Inter-Bold.ttf"
				"${INTER_EXTRACT_DIR}/*/*/*/*/Inter-Bold.ttf")

			if(INTER_SRC_REGULAR AND INTER_SRC_BOLD)
				list(GET INTER_SRC_REGULAR 0 INTER_REGULAR_PATH)
				list(GET INTER_SRC_BOLD 0 INTER_BOLD_PATH)
				file(MAKE_DIRECTORY "${INTER_FONTS_DIR}")
				file(COPY "${INTER_REGULAR_PATH}" "${INTER_BOLD_PATH}" DESTINATION "${INTER_FONTS_DIR}")
				message(STATUS "Inter font: installed to base/fonts/")
			else()
				message(WARNING "Inter font: could not find Inter-Regular.ttf / Inter-Bold.ttf in archive. Place them manually in base/fonts/")
			endif()

			file(REMOVE_RECURSE "${INTER_EXTRACT_DIR}")
			file(REMOVE "${INTER_ZIP}")
		else()
			message(WARNING "Inter font: download failed. Place Inter-Regular.ttf and Inter-Bold.ttf in base/fonts/ manually.")
		endif()
	else()
		message(STATUS "Inter font: found in base/fonts/")
	endif()
endif()
