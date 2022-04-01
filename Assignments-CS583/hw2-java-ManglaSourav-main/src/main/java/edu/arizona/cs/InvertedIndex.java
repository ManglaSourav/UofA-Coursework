package edu.arizona.cs;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.*;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

class Docs {
    String docId;
    HashMap<Integer, ArrayList<Integer>> positions;
    ArrayList<Integer> result = new ArrayList<>();

    public Docs() {
        positions = new HashMap<>();
    }

    public void addTokenPosition(Integer docId, int position) {
        if (positions.containsKey(docId)) {
            ArrayList itemPositionsArray = positions.get(docId);
            itemPositionsArray.add(position);
            positions.put(docId, itemPositionsArray);
        } else {
            ArrayList<Integer> itemPositionsArray = new ArrayList<>();
            itemPositionsArray.add(position);
            positions.put(docId, itemPositionsArray);
        }
    }

    @Override
    public String toString() {
        String docResult = new String();
        docResult = "<";
        if (docId != null) {
            docResult += docId;
        }

        if (result.size() > 0) {
            for (int i = 0; i < result.size(); i++) {
                docResult += ", " + result.get(i);
            }
        }

        docResult += ">";
        return docResult;
    }

}

public class InvertedIndex {

    boolean indexExists = false;
    String inputFilePath = "";
    Map<String, Docs> positionalIndex;

    public InvertedIndex(String inputFile) {
        inputFilePath = inputFile;
        try {
            BufferedReader br = new BufferedReader(new FileReader(inputFilePath));
            positionalIndex = new HashMap<>();
            String docs;
            while ((docs = br.readLine()) != null) {
                List<String> document = Arrays.asList(docs.split("\\s"));
                add(document);
            }
        } catch (IOException e) {
            e.printStackTrace();
        }

    }

    public void add(List<String> doc) {
        Pattern p = Pattern.compile("\\d+");
        Matcher m = p.matcher(doc.get(0));
        m.find();
        String docId = m.group(); //doc.get(0);//
        List<String> terms = doc.subList(1, doc.size());
        Iterator i = terms.iterator();
        String token;
        Docs localDoc;
        int position = 0;
        while (i.hasNext()) {
            token = (String) i.next();
            if (positionalIndex.containsKey(token)) {
                localDoc = positionalIndex.get(token);
            } else {
                localDoc = new Docs();
                positionalIndex.put(token, localDoc);
            }
            localDoc.addTokenPosition(Integer.valueOf(docId), position);
            position++;
        }
    }

    public static void main(String[] args) throws IOException {
        try {
            String fileName = "./src/main/resources/Docs.txt";
            String query = "schizophrenia /4 drug";
            InvertedIndex objInvertedIndex = new InvertedIndex(fileName);
            Document[] ans = objInvertedIndex.runQ8_1_1(query);

        } catch (Exception ex) {
            System.out.println(ex.getMessage());
        }
    }

    public Document[] positionalIntersect(HashMap<Integer, ArrayList<Integer>> p1, Integer k, HashMap<Integer, ArrayList<Integer>> p2) {

        ArrayList<Document> answer = new ArrayList<>();
        Iterator p1Iterator = p1.keySet().iterator();
        Iterator p2Iterator = p2.keySet().iterator();
        Integer docP1 = (Integer) p1Iterator.next();
        Integer docP2 = (Integer) p2Iterator.next();

        while (docP1 != null && docP2 != null) {
            if (docP1 == docP2) {
                ArrayList<Integer> l = new ArrayList<>();
                ArrayList<Integer> pp1 = p1.get(docP1);
                ArrayList<Integer> pp2 = p2.get(docP2);
                int pp1Counter = 0;
                while (pp1Counter < pp1.size()) {
                    int pp2Counter = 0;
                    while (pp2Counter < pp2.size()) {
                        if (Math.abs(pp1.get(pp1Counter) - pp2.get(pp2Counter)) <= k) {
                            if (!l.contains(pp2.get(pp2Counter))) {
                                l.add(pp2.get(pp2Counter));
                            }
                        } else if (pp2.get(pp2Counter) > pp1.get(pp1Counter)) {
                            break;
                        }
                        pp2Counter++;
                    }
                    while (!l.isEmpty() && Math.abs(l.get(0) - pp1.get(pp1Counter)) > k) {
                        l.remove(0);
                    }
                    for (Integer ps : l) {
                        Document newDoc = new Document("Doc" + String.valueOf(docP1), pp1.get(pp1Counter), ps);
                        answer.add(newDoc);
                    }
                    pp1Counter++;
                }
                if (p1Iterator.hasNext()) {
                    docP1 = (Integer) p1Iterator.next();
                } else {
                    docP1 = null;
                }
                if (p2Iterator.hasNext()) {
                    docP2 = (Integer) p2Iterator.next();
                } else {
                    docP2 = null;
                }
            } else if (docP1 < docP2) {
                if (p1Iterator.hasNext()) {
                    docP1 = (Integer) p1Iterator.next();
                } else {
                    docP1 = null;
                }
            } else {
                if (p2Iterator.hasNext()) {
                    docP2 = (Integer) p2Iterator.next();
                } else {
                    docP2 = null;
                }
            }
        }

        return answer.toArray(new Document[0]);

    }

    public Document[] positionalDirectionalIntersect(HashMap<Integer, ArrayList<Integer>> p1, Integer k, HashMap<Integer, ArrayList<Integer>> p2) {

        ArrayList<Document> answer = new ArrayList<>();
        Iterator p1Iterator = p1.keySet().iterator();
        Iterator p2Iterator = p2.keySet().iterator();
        Integer docP1 = (Integer) p1Iterator.next();
        Integer docP2 = (Integer) p2Iterator.next();

        while (docP1 != null && docP2 != null) {
            if (docP1 == docP2) {
                ArrayList<Integer> l = new ArrayList<>();
                ArrayList<Integer> pp1 = p1.get(docP1);
                ArrayList<Integer> pp2 = p2.get(docP2);
                int pp1Counter = 0;
                while (pp1Counter < pp1.size()) {
                    int pp2Counter = 0;
                    System.out.println(" ");
                    System.out.println("l array " + l);
                    while (pp2Counter < pp2.size()) {

                        if (Math.abs(pp1.get(pp1Counter) - pp2.get(pp2Counter)) <= k && (pp1.get(pp1Counter) < pp2.get(pp2Counter))) {
                            if (!l.contains(pp2.get(pp2Counter))) {
                                l.add(pp2.get(pp2Counter));
                            }
                        } else if (pp2.get(pp2Counter) > pp1.get(pp1Counter)) {
                            break;
                        }
                        pp2Counter++;
                    }
                    System.out.println(" ");
                    System.out.println("l array " + l);
                    System.out.println("pp1 counter " + pp1.get(pp1Counter));
                    while (!l.isEmpty() && Math.abs(l.get(0) - pp1.get(pp1Counter)) <= k && (l.get(0) < pp1.get(pp1Counter))) {
                        System.out.println("To be removed i come here " + l.get(0));
                        l.remove(0);
                    }
                    for (Integer ps : l) {
                        Document newDoc = new Document("Doc" + String.valueOf(docP1), pp1.get(pp1Counter), ps);
                        answer.add(newDoc);
                    }
                    pp1Counter++;
                }
                if (p1Iterator.hasNext()) {
                    docP1 = (Integer) p1Iterator.next();
                } else {
                    docP1 = null;
                }
                if (p2Iterator.hasNext()) {
                    docP2 = (Integer) p2Iterator.next();
                } else {
                    docP2 = null;
                }
            } else if (docP1 < docP2) {
                if (p1Iterator.hasNext()) {
                    docP1 = (Integer) p1Iterator.next();
                } else {
                    docP1 = null;
                }
            } else {
                if (p2Iterator.hasNext()) {
                    docP2 = (Integer) p2Iterator.next();
                } else {
                    docP2 = null;
                }
            }
        }

        return answer.toArray(new Document[0]);

    }

    public Document[] runQ8_1_1(String query) throws java.io.FileNotFoundException, ArrayIndexOutOfBoundsException {
        query = query.toLowerCase();
        String[] tokens = query.split("[ /\"]+");
        return positionalIntersect(positionalIndex.get(tokens[0]).positions, Integer.valueOf(tokens[1]), positionalIndex.get(tokens[2]).positions);
    }

    public Document[] runQ8_1_2(String query) throws java.io.FileNotFoundException, ArrayIndexOutOfBoundsException {
        //check if index exists, else create one
        query = query.toLowerCase();
        String[] tokens = query.split("[ /\"]+");
        return positionalIntersect(positionalIndex.get(tokens[0]).positions, Integer.valueOf(tokens[1]), positionalIndex.get(tokens[2]).positions);
    }

    public Document[] runQ8_2_directional(String query) throws java.io.FileNotFoundException, ArrayIndexOutOfBoundsException {
        query = query.toLowerCase();
        String[] tokens = query.split("[ /\"]+");
        return positionalDirectionalIntersect(positionalIndex.get(tokens[0]).positions, Integer.valueOf(tokens[1]), positionalIndex.get(tokens[2]).positions);
    }

}
