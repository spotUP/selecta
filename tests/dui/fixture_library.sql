-- Fixture library for the desktop UI tests (DUI-P0-1). Plain SQL over schema.c tables; loaded through the real
-- library API at implement time (DUI-P2-4). Albums 1 to 3 carry artwork rows (4x4 PNG), album 4 has none.
BEGIN;
INSERT INTO albums(id, name, artist, year, source) VALUES(1, 'Lotus Turbo Challenge', 'Chris Huelsbeck', 1992, 'local');
INSERT INTO artwork(album_id, kind, data) VALUES(1, 'cover', X'89504E470D0A1A0A0000000D494844520000000400000004080200000026930929000000104944415478DA6338A1A101470CC47100D1431181A0D36E640000000049454E44AE426082');
INSERT INTO albums(id, name, artist, year, source) VALUES(2, 'Monkey Island Theme (Extended Remaster Edition With A Very Long Title)', 'Michael Land', 1990, 'local');
INSERT INTO artwork(album_id, kind, data) VALUES(2, 'cover', X'89504E470D0A1A0A0000000D494844520000000400000004080200000026930929000000104944415478DA63D0A83801470CC471003F5216810FBFDE8B0000000049454E44AE426082');
INSERT INTO albums(id, name, artist, year, source) VALUES(3, 'Strange Sounds from Beyond', 'Jogeir Liljedahl', 1991, 'local');
INSERT INTO artwork(album_id, kind, data) VALUES(3, 'cover', X'89504E470D0A1A0A0000000D494844520000000400000004080200000026930929000000104944415478DA63D0581000470CC47100C4C311817A58E0950000000049454E44AE426082');
INSERT INTO albums(id, name, artist, year, source) VALUES(4, 'Ünïcødé Álbum åäö', 'Zoë Björk', 1994, 'local');
INSERT INTO songs(id, key, source, path, name, artist, album, format, year, genre, duration_ms, added) VALUES(1, 'fixture:1', 'local', 'fixture/1.mod', 'Intro', 'Chris Huelsbeck', 'Lotus Turbo Challenge', 'mod', 1992, 'Chiptune', 120000, 1700000000);
INSERT INTO songs(id, key, source, path, name, artist, album, format, year, genre, duration_ms, added) VALUES(2, 'fixture:2', 'local', 'fixture/2.mod', 'Main Theme', 'Michael Land', 'Monkey Island Theme (Extended Remaster Edition With A Very Long Title)', 'mod', 1990, 'Chiptune', 150000, 1700000001);
INSERT INTO songs(id, key, source, path, name, artist, album, format, year, genre, duration_ms, added) VALUES(3, 'fixture:3', 'local', 'fixture/3.xm', 'Title Tune With An Extremely Long Name That Must Ellipsize In A Narrow Column', 'Jogeir Liljedahl', 'Strange Sounds from Beyond', 'xm', 1991, 'Chiptune', 180000, 1700000002);
INSERT INTO songs(id, key, source, path, name, artist, album, format, year, genre, duration_ms, added) VALUES(4, 'fixture:4', 'local', 'fixture/4.it', 'Cafe Del Mar – Café Mix', 'Zoë Björk', 'Ünïcødé Álbum åäö', 'it', 1994, 'Chiptune', 210000, 1700000003);
INSERT INTO songs(id, key, source, path, name, artist, album, format, year, genre, duration_ms, added) VALUES(5, 'fixture:5', 'local', 'fixture/5.sid', 'Bass Line', 'Chris Huelsbeck', 'Lotus Turbo Challenge', 'sid', 1992, 'Chiptune', 240000, 1700000004);
INSERT INTO songs(id, key, source, path, name, artist, album, format, year, genre, duration_ms, added) VALUES(6, 'fixture:6', 'local', 'fixture/6.ahx', 'Outro', 'Michael Land', 'Monkey Island Theme (Extended Remaster Edition With A Very Long Title)', 'ahx', 1990, 'Chiptune', 270000, 1700000005);
COMMIT;
