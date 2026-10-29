-- I678j: three albums (Alb1 played 3 times, Alb3 twice, Alb2 once), one playlist "Mix" holding a Alb2 song, one liked song.
INSERT INTO songs(key, source, path, name, artist, album, added) VALUES
('local:a.mod','local','a.mod','a','Ann','Alb1',100),('local:b.mod','local','b.mod','b','Bob','Alb2',200),
('local:c.mod','local','c.mod','c','Cy','Alb3',300);
INSERT INTO albums(name, artist) VALUES('Alb1','Ann'),('Alb2','Bob'),('Alb3','Cy');
INSERT INTO history(key, played_at) VALUES('local:a.mod',1000),('local:a.mod',1100),('local:a.mod',1200),
('local:c.mod',1300),('local:c.mod',1400),('local:b.mod',1500);
INSERT INTO playlists(id, name) VALUES(1,'Mix');
INSERT INTO playlist_entries(playlist_id, pos, key) VALUES(1,0,'local:b.mod');
INSERT INTO liked(key, at) VALUES('local:a.mod',900);
-- I678j chips: one SID song and one tracker song, each its own album, played once (the other three songs have no format: Music).
INSERT INTO songs(key, source, path, name, artist, album, format, added) VALUES
('local:d.sid','local','d.sid','d','Dee','Alb4','sid',400),('local:e.mod','local','e.mod','e','Eve','Alb5','mod',500);
INSERT INTO albums(name, artist) VALUES('Alb4','Dee'),('Alb5','Eve');
INSERT INTO history(key, played_at) VALUES('local:d.sid',1600),('local:e.mod',1700);
