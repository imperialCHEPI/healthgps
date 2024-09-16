include_guard()

# Get the current git reference with "git describe"
function(get_version_git VERSION_VAR)
    execute_process(
        COMMAND git describe --tags
        RESULT_VARIABLE retval
        OUTPUT_VARIABLE outval)

    if(${retval} EQUAL 0)
        string(STRIP "${outval}" outval)
        set("${VERSION_VAR}"
            "${outval}"
            PARENT_SCOPE)
    else()
        # Didn't work... maybe git isn't installed?
        message(WARNING "Failed to get current git ref")
    endif()
endfunction()
