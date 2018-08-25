
local type_list = {
	test = {
		cols                = 300,
		rows				= 300,
		quality			    = 75,
		rotate              = 90,
		gray			    = 1,
        format              = 'webp',
    },
    head = {
        cols = 300,
        rows = 300,
        format = 'jpeg'
    },
    green = {
        cols = 400,
        rows = 200,
        format = 'jpeg'
    },
    art_title = {
        cols = 172,
        rows = 120,
        format = 'jpeg'
    },
    ppt = {
        cols = 500,
        rows = 300,
        format = 'jpeg'
    },
    thumb = {
        cols = 300,
        rows = 300,
        format = 'jpeg'
    },
    d_one = {
        cols = 400,
        rows = 250,
        format = 'jpeg'
    }
}
local OK                    = 1

function f() --The main function must be named f() for zimg-lua
    local code = -1
    local rtype = zimg.type() --Get the request type from url argument

    local arg = type_list[rtype] --Find the type details
    if not arg then
        zimg.ret(code)
    end

    local ret = zimg.scale(arg.cols, arg.rows) --Scale image
    if ret ~= OK then
        zimg.ret(code)
    end

    if arg.rotate then
        ret = zimg.rotate(arg.rotate) --Rotate image
        if ret ~= OK then
            zimg.ret(code)
        end
    end

    if arg.gray and arg.gray == 1 then
        ret = zimg.gray() --Grayscale image
        if ret ~= OK then
            zimg.ret(code)
        end
    end

    if arg.quality and zimg.quality() > arg.quality then
        ret = zimg.set_quality(arg.quality) --Set quality of image
        if ret ~= OK then
            zimg.ret(code)
        end
    end

    if arg.format then
        ret = zimg.set_format(arg.format) --Set format
        if ret ~= OK then
            zimg.ret(code)
        end
    end

    code = OK
    zimg.ret(code) --Return the result to zimg
end
