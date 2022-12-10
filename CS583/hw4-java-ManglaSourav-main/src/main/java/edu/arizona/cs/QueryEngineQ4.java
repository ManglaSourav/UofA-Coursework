package edu.arizona.cs;

import java.io.*;
import java.util.*;

import edu.stanford.nlp.simple.*;

public class QueryEngineQ4 {
    String inputFilePath = "";
    HashMap<String, List<String>> docs = new HashMap<>();
    ;

    public QueryEngineQ4(String inputFile) {
        inputFilePath = "./src/main/resources/" + inputFile;
        buildModel();
    }

    public static void main(String[] args) {
        try {
            QueryEngineQ4 q4 = new QueryEngineQ4("input.txt");
        } catch (Exception ex) {
            System.out.println(ex.getMessage());
        }
    }

    private void buildModel() {
        try {
            BufferedReader br = new BufferedReader(new FileReader(inputFilePath));
            String doc;
            while ((doc = br.readLine()) != null) {
                String docId = doc.substring(0, 4);
                String content = doc.substring(4, doc.length() - 1);
                List<String> tokens = new Sentence(content).lemmas();
                docs.put(docId, tokens);
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }


    public List<ResultClass> runQ4_3_with_smoothing(String[] query) throws java.io.FileNotFoundException {
        List<ResultClass> ans = new ArrayList<ResultClass>();
        for (String docId : docs.keySet()) {
            List<String> tokens = docs.get(docId);
            double score = 1;
            for (String query_token : query) {
                int Ld = tokens.size();
                int TFtd = Collections.frequency(tokens, query_token);
                double t = 0;
                double CFt = 0;

                for (String docid : docs.keySet()) {
                    CFt += Collections.frequency(docs.get(docid), query_token);
                    t += docs.get(docid).size();
                }
                double PtMc = CFt / t;
                double param = (TFtd + 0.5 * PtMc) / (Ld + 0.5);
                score *= param;
            }
            ResultClass result = new ResultClass();
            result.docScore = score;
            result.DocName = new Document(docId);
            ans.add(result);
        }
        return ans;
    }

    public List<ResultClass> runQ4_3_without_smoothing(String[] query) throws java.io.FileNotFoundException {

        List<ResultClass> ans = new ArrayList<ResultClass>();

        for (String docId : docs.keySet()) {
            List<String> tokens = docs.get(docId);
            double score = 1;
            for (String query_token : query) {
                int d = tokens.size();
                int TFtd = Collections.frequency(tokens, query_token);
                double PtMd = (double) TFtd / d;
                score *= PtMd;
            }
            ResultClass result = new ResultClass();
            result.docScore = score;
            result.DocName = new Document(docId);
            ans.add(result);
        }
        return ans;
    }

}

