
function Move(x, y, piece_id) {
    if (arguments.length == 3)
	this.m = x << 4 | y | piece_id << 8;
    else if (typeof x == "number")
	this.m = x;
    else if (x == "----")
	this.m = 0xffff;
    else {
	var xy = parseInt(x.substring(0, 2), 16);
	var blk = 117 - x.charCodeAt(2); // 117 is 'u'
	var dir = parseInt(x.substring(3));
	this.m = xy - 0x11 | blk << 11 | dir << 8;
    }
}

Move.prototype.x = function() { return this.m >> 4 & 0xf; }
Move.prototype.y = function() { return this.m & 0xf; }
Move.prototype.pieceId = function() { return this.m >> 8; }
Move.prototype.blockId = function() { return this.m >> 11; }
Move.prototype.direction = function() { return this.m >> 8 & 0x7; }
Move.prototype.isPass = function() { return this.m == 0xffff; }
Move.prototype.fourcc = function() {
    if (this.isPass())
	return "----";
    return ((this.m & 0xff) + 0x11).toString(16) +
	String.fromCharCode(117 - this.blockId()) +
	this.direction();
}

Move.INVALID_MOVE = new Move(0xfffe);
Move.PASS = new Move(0xffff);


function Board() {
    this.square = [];
    for (var y = 0; y < 14; y++) {
	this.square[y] = [];
	for (var x = 0; x < 14; x++)
	    this.square[y][x] = 0;
    }
    this.square[4][4] = Board.VIOLET_EDGE;
    this.square[9][9] = Board.ORANGE_EDGE;
    this.turn = 0;
    this.used = new Array(21*2);
}

Board.VIOLET_MASK = 0x07;
Board.ORANGE_MASK = 0x70;
Board.VIOLET_EDGE = 0x01;
Board.ORANGE_EDGE = 0x10;
Board.VIOLET_SIDE = 0x02;
Board.ORANGE_SIDE = 0x20;
Board.VIOLET_BLOCK = 0x04;
Board.ORANGE_BLOCK = 0x40;

Board.prototype.inBounds = function(x, y) {
    return (x >= 0 && y >= 0 && x < 14 && y < 14);
}
Board.prototype.at = function(x, y) { return this.square[y][x]; }
Board.prototype.isViolet = function() { return (this.turn & 1) == 0; }

Board.prototype.isValidMove = function(move) {
    if (move.isPass())
	return true;

    if (this.used[move.blockId() + (this.isViolet() ? 0 : 21)])
	return false;

    var rot = blockSet[move.blockId()].rotations[move.direction()];
    var px = move.x() + rot.offsetX;
    var py = move.y() + rot.offsetY;
    var piece = rot.piece;

    if (px + piece.minx < 0 || px + piece.maxx >= 14 ||
	py + piece.miny < 0 || py + piece.maxy >= 14 ||
	!this._isMovable(px, py, piece))
	return false;

    for (var i = 0; i < piece.size; i++) {
	var x = px + piece.coords[i][0];
	var y = py + piece.coords[i][1];
	if (this.square[y][x] & (this.isViolet() ? Board.VIOLET_EDGE : Board.ORANGE_EDGE))
	    return true;
    }
    return false;
}

Board.prototype.doMove = function(move) {
    if (move.isPass()) {
	this.doPass();
	return;
    }

    var rot = blockSet[move.blockId()].rotations[move.direction()];
    var px = move.x() + rot.offsetX;
    var py = move.y() + rot.offsetY;
    var piece = rot.piece;

    var block = this.isViolet() ? Board.VIOLET_BLOCK : Board.ORANGE_BLOCK;
    var side_bit = this.isViolet() ? Board.VIOLET_SIDE : Board.ORANGE_SIDE;
    var edge_bit = this.isViolet() ? Board.VIOLET_EDGE : Board.ORANGE_EDGE;

    for (var i = 0; i < piece.size; i++) {
	var x = px + piece.coords[i][0];
	var y = py + piece.coords[i][1];
	this.square[y][x] |= block;
	if (this.inBounds(x-1, y)) this.square[ y][x-1] |= side_bit;
	if (this.inBounds(x, y-1)) this.square[y-1][ x] |= side_bit;
	if (this.inBounds(x+1, y)) this.square[ y][x+1] |= side_bit;
	if (this.inBounds(x, y+1)) this.square[y+1][ x] |= side_bit;
	if (this.inBounds(x-1,y-1)) this.square[y-1][x-1] |= edge_bit;
	if (this.inBounds(x+1,y-1)) this.square[y-1][x+1] |= edge_bit;
	if (this.inBounds(x-1,y+1)) this.square[y+1][x-1] |= edge_bit;
	if (this.inBounds(x+1,y+1)) this.square[y+1][x+1] |= edge_bit;
    }

    this.used[move.blockId() + (this.isViolet() ? 0 : 21)] = true;
    this.turn++;
}

Board.prototype.doPass = function() { this.turn++; }

Board.prototype.violetScore = function() {
    var score = 0;
    for (var i = 0; i < 21; i++) {
	if (this.used[i])
	    score += blockSet[i].size;
    }
    return score;
}

Board.prototype.orangeScore = function() {
    var score = 0;
    for (var i = 0; i < 21; i++) {
	if (this.used[21 + i])
	    score += blockSet[i].size;
    }
    return score;
}

Board.prototype._isMovable = function(px, py, piece) {
    var mask = this.isViolet() ? (Board.VIOLET_BLOCK|Board.VIOLET_SIDE|Board.ORANGE_BLOCK)
	: (Board.ORANGE_BLOCK|Board.ORANGE_SIDE|Board.VIOLET_BLOCK);

    for (var i = 0; i < piece.size; i++) {
	var x = px + piece.coords[i][0];
	var y = py + piece.coords[i][1];
	if (this.square[y][x] & mask)
	    return false;
    }
    return true;
}

Board.prototype.isUsed = function(player, blockId) {
    return this.used[blockId + player * 21];
}

Board.prototype.canMove = function() {
    var player = this.turn % 2;
    for (var p in pieceSet) {
	var id = pieceSet[p].id
	if (this.used[(id >> 3) + player * 21])
	    continue;
	for (var y = 0; y < 14; y++) {
	    for (var x = 0; x < 14; x++) {
		if (this.isValidMove(new Move(x, y, id)))
		    return true;
	    }
	}
    }
    return false;
}
