package raylib
import "core:strings"
import _c "core:c"

DrawStringRec :: proc(
        font : Font,
        text : string,
        rec : Rectangle,
        fontSize : _c.float,
        spacing : _c.float,
        wordWrap : bool,
        color : Color)
{
    ctext : cstring = strings.clone_to_cstring(text);
    DrawTextRec(font, ctext, rec, fontSize, spacing, wordWrap, color);
}

DrawString :: proc(
        text : string,
        posX : _c.int,
        posY : _c.int,
        fontSize : _c.int,
        color : Color
    )
{
    ctext : cstring = strings.clone_to_cstring(text);
    DrawText(ctext, posX, posY, fontSize, color);
}

MeasureString :: proc(text : string, fontSize : _c.int) -> i32
{
    ctext : cstring = strings.clone_to_cstring(text);
    return MeasureText(ctext, fontSize);
}