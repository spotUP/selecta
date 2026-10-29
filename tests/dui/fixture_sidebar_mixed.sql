-- I678b: Your Library kinds fixture (same rows as tests/engine/library_items_test.c): 2 playlists, 3 albums, 3 artists,
-- 3 composers (two named plus the Unknown group), 1 non-local source; with the 3 pinned rows the list holds 15 items.
INSERT INTO songs(key, source, path, name, artist, album, added) VALUES
('local:a.mod','local','a.mod','a','Ann','Alb1',100),('local:b.mod','local','b.mod','b','Ann','Alb1',200),
('local:c.mod','local','c.mod','c','Bob','Alb2',300),('modland:d.mod','modland','d.mod','d','Bob',NULL,400);
INSERT INTO albums(name, artist) VALUES('Alb1','Ann'),('Alb2','Bob'),('Alb3','Cy');
INSERT INTO song_composers(song_key, canon, display) VALUES('local:a.mod','x','X'),('local:b.mod','x','X'),('local:c.mod','y','Y');
INSERT INTO playlists(name) VALUES('Mix'),('Chill');
INSERT INTO playlist_entries(playlist_id, pos, key) VALUES(1,0,'local:a.mod'),(1,1,'local:b.mod');
INSERT INTO liked(key, at) VALUES('local:a.mod',50);
INSERT INTO history(key, played_at) VALUES('local:a.mod',500),('local:c.mod',700),('local:a.mod',600);
