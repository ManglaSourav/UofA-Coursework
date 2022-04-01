package edu.arizona.cs;

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.*;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class InvertedIndex {
    boolean indexExists = false;
    String inputFilePath = "";
    private final TreeMap<String, LinkedList<String>> postingsList;

    public InvertedIndex(String inputFile) throws java.io.IOException {
        inputFilePath = inputFile;
        BufferedReader br = new BufferedReader(new FileReader(inputFilePath));
        postingsList = new TreeMap<>();
        String docs;
        while ((docs = br.readLine()) != null){
            List<String> document = Arrays.asList(docs.split("\\s"));
            add(document);
        }
    }

    public void add(List<String> doc) {
        Pattern p = Pattern.compile("\\d+");
        Matcher m = p.matcher(doc.get(0));
        m.find();
        String docId =  m.group(); //doc.get(0);//
//        System.out.println("group id " + docId);
        List<String> terms = doc.subList(1, doc.size());
//        System.out.println("Terms : " + terms);

        Iterator i = terms.iterator();
        String token;
        LinkedList localPostingList;
        while (i.hasNext()) {
            token = (String) i.next();
            if (postingsList.containsKey(token)) {
                localPostingList = postingsList.get(token);
            }else {
                /* otherwise, add the term and then create new posting list */
                localPostingList = new LinkedList();
                postingsList.put(token, localPostingList);
            }
            if(!localPostingList.contains(docId)){
                localPostingList.add(docId);
            }
        }
    }

    public TreeMap<String, LinkedList<String>> getPostingsList() {
        return postingsList;
    }

    public static void main(String[] args ) {
        try {
            String fileName = "src/main/resources/Docs.txt";
            String query = "(drug OR treatment) AND schizophrenia";
            InvertedIndex objInvertedIndex = new InvertedIndex(fileName);
            System.out.println(objInvertedIndex.getPostingsList());
            String[]  ans2 = objInvertedIndex.runQ5_3(query);
            System.out.println(Arrays.toString(ans2));
        }
        catch (Exception ex) {
            System.out.println(ex.getMessage());
        }
    }

    public LinkedList orOperation(LinkedList p1PostingsList, LinkedList p2PostingsList) throws java.io.IOException {
        LinkedList<String> answer = new LinkedList<>();

        Iterator p1 = p1PostingsList.iterator();
        Iterator p2 = p2PostingsList.iterator();

        String docIdP1 = (String) p1.next();
        String docIdP2 = (String) p2.next();

        while (!docIdP1.isEmpty() || !docIdP2.isEmpty()) {
            if(docIdP1.isEmpty()){
                answer.add(docIdP2);
                if(p2.hasNext())  docIdP2 = (String) p2.next();
                else docIdP2 = "";
            }else if(docIdP2.isEmpty()){
                answer.add(docIdP1);
                if(p1.hasNext())  docIdP1 = (String) p1.next();
                else docIdP1 = "";
            }else if(docIdP1.equals(docIdP2)){
                answer.add(docIdP1);
                if(p1.hasNext())  docIdP1 = (String) p1.next();
                else docIdP1 = "";
                if(p2.hasNext()) docIdP2 = (String) p2.next();
                else docIdP2 = "";
            }else if(Integer.parseInt(docIdP1) < Integer.parseInt(docIdP2)){
                answer.add(docIdP1);
                if(p1.hasNext()) docIdP1 = (String) p1.next();
                else docIdP1 = "";
            }else if(Integer.parseInt(docIdP1) > Integer.parseInt(docIdP2)){
                answer.add(docIdP2);
                if(p2.hasNext()) docIdP2 = (String) p2.next();
                else docIdP2 = "";
            }
        }
        return answer;
    }

    public LinkedList andOperation(LinkedList p1PostingsList, LinkedList p2PostingsList) throws java.io.IOException {

        LinkedList<String> answer = new LinkedList<>();
        Iterator p1 = p1PostingsList.iterator();
        Iterator p2 = p2PostingsList.iterator();

        String docIdP1 = (String) p1.next();
        String docIdP2 = (String) p2.next();

        while (!docIdP1.isEmpty() && !docIdP2.isEmpty()) {
            if(docIdP1.equals(docIdP2)){
                answer.add(docIdP1);
                if(p1.hasNext())  docIdP1 = (String) p1.next();
                else docIdP1 = "";

                if(p2.hasNext()) docIdP2 = (String) p2.next();
                else docIdP2 = "";

            }else if(Integer.parseInt(docIdP1) < Integer.parseInt(docIdP2)){
                if(p1.hasNext()) docIdP1 = (String) p1.next();
                else docIdP1 = "";
            }else{
                if(p2.hasNext()) docIdP2 = (String) p2.next();
                else docIdP2 = "";
            }
        }
        return answer;
    }

    public String[] runQ5_1(String query) throws java.io.IOException {
        query = query.toLowerCase();
        List<String> token = Arrays.asList(query.split("\\s"));
        LinkedList answer = andOperation(postingsList.get(token.get(0)), postingsList.get(token.get(2)));
        ArrayList<String> finalAnswer = new ArrayList<String>();
        for (int i = 0; i < answer.size(); i++) {
            finalAnswer.add("Doc"+answer.get(i));
        }
        return finalAnswer.toArray(new String[0]);
    }

    public String[] runQ5_2(String query) throws java.io.IOException {
        query = query.toLowerCase();
        List<String> token = Arrays.asList(query.split("\\s"));
        LinkedList answer = orOperation(postingsList.get(token.get(0)), postingsList.get(token.get(2)));
        ArrayList<String> finalAnswer = new ArrayList<String>();
        for (int i = 0; i < answer.size(); i++) {
            finalAnswer.add("Doc"+answer.get(i));
        }
        return finalAnswer.toArray(new String[0]);
    }

    public String[] runQ5_3(String query) throws java.io.IOException {
        query = "(" + query + ")";
        query = query.toLowerCase().replace("(","( ").replace(")"," )");
        StringTokenizer tokens = new StringTokenizer(query," ");

        Stack<String> tokenStack = new Stack<>();
        Stack<LinkedList> answerStack = new Stack<>();

        while (tokens.hasMoreTokens()) {
            String token = tokens.nextToken();
            if (tokens.equals(" ")) {
                continue;
            }else if(token.equals(")")){
                String op1 = tokenStack.pop();
                String op = tokenStack.pop();
                String op2 = tokenStack.pop();
                tokenStack.pop(); //pop for ")"
//                System.out.println("op1 " + op1 + " " + op + " "+ op2);
                LinkedList localAns;

                LinkedList<String> op1PostingList;
                LinkedList<String> op2PostingList;

                if(op1.equals("answerStack")){
                    op1PostingList = answerStack.pop();
                }else{
                    op1PostingList = postingsList.get(op1);
                }

                if(op2.equals("answerStack")){
                    op2PostingList = answerStack.pop();
                }else{
                    op2PostingList = postingsList.get(op2);
                }

                if(op.equals("or")){
                    localAns = orOperation(op1PostingList, op2PostingList);
                }else{
                    localAns = andOperation(op1PostingList, op2PostingList);
                }

                answerStack.push(localAns);
                tokenStack.push("answerStack");
//                System.out.println(tokenStack);
//                System.out.println(answerStack);
            }else if(!token.isEmpty()){
                   tokenStack.push(token);
            }

        }
        LinkedList<String > answer = answerStack.pop();
        ArrayList<String> finalAnswer = new ArrayList<String>();
        for (int i = 0; i < answer.size(); i++) {
            finalAnswer.add("Doc"+answer.get(i));
        }
        return finalAnswer.toArray(new String[0]);
    }

}
