-- I678c: three albums with one song each; only album 3 was ever played (at 1000, long before any real clock reading),
-- so Recents lists Alb3 first until an album page is opened.
INSERT INTO songs(key, source, path, name, artist, album, added) VALUES
('local:a.mod','local','a.mod','a','Ann','Alb1',100),('local:b.mod','local','b.mod','b','Bob','Alb2',200),
('local:c.mod','local','c.mod','c','Cy','Alb3',300);
INSERT INTO albums(name, artist) VALUES('Alb1','Ann'),('Alb2','Bob'),('Alb3','Cy');
INSERT INTO history(key, played_at) VALUES('local:c.mod',1000);
