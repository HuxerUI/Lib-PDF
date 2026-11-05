huxerui_add_app(pdf_preview
    SOURCES main.cpp
    BUNDLE_NAME "HuxerUI PDF"
    BUNDLE_IDENTIFIER "org.huxerui.pdf.preview")
huxerui_use_library(pdf_preview TARGET HuxerUI::PDF)
target_compile_definitions(pdf_preview PRIVATE PDF_SAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../../tests/fixtures/sample.pdf")
target_compile_options(pdf_preview PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
huxerui_pdf_copy_runtime(pdf_preview)
