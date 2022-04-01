package edu.arizona.cs;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.net.URISyntaxException;
import java.util.*;
import java.io.File;
import java.util.stream.Collectors;

public class QueryEngineQ5 {
    String inputFilePath = "";
    HashMap<String, List<String>> trainData;
    HashMap<String, List<String>> testData;
    String[] classes;
    HashSet<String> vocabulary;
    HashMap<String, Double> priorProbabilities;
    HashMap<String, HashMap<String, Double>> condProbabilities;

    public QueryEngineQ5() {
        inputFilePath = "spamDataset";
        trainData = new HashMap<>();
        testData = new HashMap<>();
        classes = new String[]{"spam", "nonspam"};
        vocabulary = new HashSet<String>();
        priorProbabilities = new HashMap<>();
        condProbabilities = new HashMap<>();
        buildModel();
    }

    public static void main(String[] args) {
        try {
            QueryEngineQ5 q5 = new QueryEngineQ5();
            q5.runQ5_2_f1score();
        } catch (Exception ex) {
            System.out.println(ex.getMessage());
        }
    }

    public void buildModel() {
        loadDataFile(inputFilePath + "/spam-train", "spam", true);
        loadDataFile(inputFilePath + "/spam-test", "spam", false);
        loadDataFile(inputFilePath + "/nonspam-train", "nonspam", true);
        loadDataFile(inputFilePath + "/nonspam-test", "nonspam", false);
        trainMultinomialNB();
    }

    public void loadDataFile(String path, String clss, boolean isTrainData) {
        try {
            ArrayList<String> data = new ArrayList<>();
            File[] files = new File(QueryEngineQ5.class.getClassLoader().getResource(path).toURI()).listFiles();

            assert files != null;
            for (File file : files) {
                BufferedReader fileContent = new BufferedReader(new FileReader(file));
                String doc = fileContent.lines().collect(Collectors.joining(System.lineSeparator()));
                String[] tempVocabulary = doc.split(" ");
                Collections.addAll(vocabulary, tempVocabulary);
                data.add(doc);
            }
            if (isTrainData) {
                trainData.put(clss, data);
            } else {
                testData.put(clss, data);
            }
        } catch (IOException | URISyntaxException e) {
            e.printStackTrace();
        }
    }

    public void trainMultinomialNB() {
        int N = 0;
        for (String c : classes) {
            N += trainData.get(c).size();
        }
        for (String c : classes) {
            int Nc = trainData.get(c).size();
            priorProbabilities.put(c, (double) Nc / N);
            String TEXTc = concatenateTextOfAllDocsInClass(trainData.get(c));
            ArrayList<String> trainingDataTokens = textToTokens(TEXTc);

            int Tct_ = trainingDataTokens.size();
            for (String token : vocabulary) {
                int Tct = Collections.frequency(trainingDataTokens, token);
                //if hashmap is already available
                HashMap<String, Double> temp = condProbabilities.get(token);
                if (temp == null) {
                    temp = new HashMap<>();
                }
                temp.put(c, (double) (Tct + 1) / (Tct_ + vocabulary.size()));
                condProbabilities.put(token, temp);
            }
        }
    }

    public ArrayList<String> textToTokens(String textData) {
        ArrayList<String> tokens = new ArrayList<>();
        Collections.addAll(tokens, textData.split(" "));
        return tokens;
    }

    public String concatenateTextOfAllDocsInClass(List<String> docs) {
        StringBuilder TEXTc = new StringBuilder();
        for (String doc : docs) {
            TEXTc.append(" ").append(doc);
        }
        return TEXTc.toString();

    }

    public double runQ5_2_f1score() {
        double f1 = 0;
        int TP = 0;
        int FP = 0;
        int TPFN = 0;
        double P = 0;
        double R = 0;

        for (String c : classes) {
            List<String> testDocs = testData.get(c);
            TPFN = testDocs.size();
            for (String d : testDocs) {
                Map.Entry<String, Double> predictedClass = applyMultinomialNB(d);
                if (predictedClass.getKey().equals(c)) {
                    TP++;
                } else {
                    FP++;
                }
            }
        }
        P = (double) TP / (TP + FP);
        R = (double) TP / TPFN;
        f1 = 2 * P * R / (P + R);
        return f1;
    }

    public Map.Entry<String, Double> applyMultinomialNB(String d) {
        ArrayList<String> W = textToTokens(d);
        HashMap<String, Double> score = new HashMap<>();
        for (String c : classes) {
            score.put(c, Math.log(priorProbabilities.get(c)));
            for (String t : W) {
                Double condP = condProbabilities.get(t).get(c);
                if (condP != null) {
                    score.put(c, score.get(c) + Math.log(condP));
                }
            }
        }
        Map.Entry<String, Double> maxScore = null;
        for (Map.Entry<String, Double> entry : score.entrySet()) {
            if (maxScore == null || entry.getValue().compareTo(maxScore.getValue()) > 0) {
                maxScore = entry;
            }
        }
        return maxScore;
    }
}
