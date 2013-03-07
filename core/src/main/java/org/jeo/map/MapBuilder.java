package org.jeo.map;

import java.io.IOException;
import java.util.Iterator;

import org.jeo.data.Dataset;
import org.jeo.data.Workspace;
import org.jeo.data.Workspaces;
import org.osgeo.proj4j.CoordinateReferenceSystem;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.vividsolutions.jts.geom.Envelope;

public class MapBuilder {

    static Logger LOG = LoggerFactory.getLogger(MapBuilder.class);

    Map map;
    boolean size = false, bounds = false, crs = false;

    public MapBuilder() {
        map = new Map();
    }

    public MapBuilder size(int width, int height) {
        map.setWidth(width);
        map.setHeight(height);
        return this;
    }

    public MapBuilder bounds(Envelope bounds) {
        //TODO: set size based on bbox aspect ratio
        map.setBounds(bounds);
        return this;
    }

    public MapBuilder crs(CoordinateReferenceSystem crs) {
        map.setCRS(crs);
        return this;
    }

    public MapBuilder layer(Dataset data) {
        return layer(data.getName(), data.getTitle(), data);
    }

    public MapBuilder layer(String name, Dataset data) {
        return layer(name, data.getTitle() != null ? data.getTitle() : name, data);
    }

    public MapBuilder layer(String name, String title, Dataset data) {
        Layer l = new Layer();
        l.setName(name);
        l.setTitle(title);
        l.setData(data);

        map.getLayers().add(l);
        return this;
    }

    public MapBuilder layer(String name, java.util.Map<String,Object> params) throws IOException {
        return layer(name, name, params);
    }

    public MapBuilder layer(String name, String title, java.util.Map<String,Object> params) 
        throws IOException {

        Workspace ws = Workspaces.create(params);
        if (ws == null) {
            throw new IllegalArgumentException("Unable to obtqin workspace: " + params);
        }

        Dataset data = ws.get(name);
        if (data == null) {
            ws.dispose();
            throw new IllegalArgumentException(
                "No dataset named " + name + " in worksoace: " + params);
        }

        layer(name, title, data);
        map.getCleanup().add(ws);
        return this;
    }

    public MapBuilder style(Stylesheet style) {
        map.setStyle(style);
        return this;
    }

    public Map map() {
        if (!bounds || !crs) {
            //set from layers
            Iterator<Layer> it = map.getLayers().iterator();
            while(it.hasNext() && !bounds && !crs) {
                Layer l = it.next();
                try {
                    if (!bounds) {
                        Envelope e = l.getData().bounds();
                        if (e != null && !e.isNull()) {
                            map.setBounds(e);
                            bounds = true;
                        }
                    }
                    if (!crs) {
                        CoordinateReferenceSystem c = l.getData().getCRS();
                        if (c != null) {
                            map.setCRS(c);
                            crs = true;
                        }
                    }
                } catch (IOException ex) {
                    LOG.debug("Error deriving bounds/crs from map layers", ex);
                }
            }
        }

        if (!size) {
            //set from bounds
            Envelope e = map.getBounds();
            if (e != null) {
                map.setWidth(Map.DEFAULT_WIDTH);
                map.setHeight((int)(map.getWidth() * e.getWidth() / e.getHeight()));
            }
        }

        return map;
    }
}
